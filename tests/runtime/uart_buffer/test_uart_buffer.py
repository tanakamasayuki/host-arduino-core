"""Tests for the device-facing UARTs (bundled with the core, HostUart.h).

`Serial1` / `Serial2` are not wired to anything outside the process: both
directions are queues program code drives, so a test owns the whole
conversation with the device it is pretending to be.

The three servicing points a real driver needs are all covered:

  - from `kPreLoop`, for a sketch that sends now and reads the reply in a
    later iteration
  - from inside the clock port's wait, for a sketch that sends and reads
    the reply before returning from `loop()` — the AT-command shape
  - from the activity hook, which fires before `write()` returns, so the
    answer is queued before the sketch's next statement and UART traffic
    keeps its place among the GPIO and SPI events around it

CR and LF are reported as '.' so one event stays on one line.
"""


def test_uart_buffer(dut):
    dut.expect("TEST start uart_buffer", timeout=10)

    # Before begin(): `operator bool` is false, the instance still knows
    # which UART it is.
    dut.expect("before: begun=0 bool=0 num=1", timeout=10)

    # begin() records what it was given — the arduino-esp32 SERIAL_8N1
    # value verbatim, so a shared sketch prints the same number on both
    # targets. Nothing enforces baud or pins.
    dut.expect("begin: begun=1 baud=9600 config=800001C rx=16 tx=17", timeout=10)

    # Nothing but a driver consumes tx, so what the sketch wrote is still
    # queued, in order.
    dut.expect("tx: written=5 waiting=5 total=5", timeout=10)
    dut.expect("drain: took=5 text=hello left=0", timeout=10)

    # And pushed bytes are what the sketch reads back.
    dut.expect("rx: available=5 peek=w total=5", timeout=10)
    dut.expect("rx: text=world available=0 read=-1", timeout=10)

    # The AT shape: sent and answered inside one call, without returning
    # to loop(). The modem saw "AT+CSQ\r\n" and the sketch read the first
    # line of the reply back.
    dut.expect("sameiter: cmd=AT[+]CSQ[.][.] reply=[+]CSQ: 24,0[.] commands=1", timeout=10)

    # With nobody playing the device the same call just times out, and the
    # command stays queued for whoever eventually drains it.
    dut.expect("unserviced: len=0 waiting=4", timeout=10)

    # Overflow drops what will not fit and says so, rather than growing
    # until the test runs out of memory. 8 of 16 bytes accepted.
    dut.expect("txoverflow: accepted=8 room=0 flag=1", timeout=10)
    dut.expect("rxoverflow: pushed=4 available=4 flag=1", timeout=10)

    # flush() drops the unread receive side only; the transmit queue is
    # the driver's and losing it would lose a command the sketch believes
    # it sent.
    dut.expect("flush: available=0 waiting=4", timeout=10)

    # Serial2 is a separate instance with its own queues.
    dut.expect("serial2: num=2 baud=115200 waiting=3 other=4", timeout=10)

    # The activity hook: the answer was pushed from inside the TX
    # notification, so all four bytes of "OK\r\n" are already readable
    # before print() returned. This is also the deadlock check — the core
    # must not hold its mutex across the callback, or pushRx would hang.
    dut.expect("activity: answered=4", timeout=10)
    dut.expect("activity: first=O trace=5 overflow=0", timeout=10)
    for line in (
        "# begin len=0",
        "# tx len=4 AT[.][.]",
        "# rx len=1 O",
        # flush() names the bytes it dropped rather than losing them
        # silently: what was left of the reply plus the two just pushed.
        "# discard len=5 K[.][.]XY",
        "# config len=0",
    ):
        dut.expect(line, timeout=10)

    # Releasing the slot stops the notifications; the queues keep working.
    dut.expect("activity: cleared_delta=0 waiting=5", timeout=10)

    # A re-begin() is a clean conversation: both queues emptied.
    dut.expect("reset: begun=1 waiting=0 available=0 total=0", timeout=10)

    # The kPreLoop shape: nothing has answered yet in the iteration that
    # sent, ...
    dut.expect("preloop: sent available=0", timeout=10)
    # ... and by the next one the driver has run exactly once.
    dut.expect("preloop: reply=OK[.] services=1 commands=1", timeout=10)

    dut.expect("TEST done", timeout=10)
