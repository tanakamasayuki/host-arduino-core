"""Tests for the clock port (cores/host/HostClock.h).

Covers the three states the port has:

  1. nothing installed — the real monotonic clock, `delay` really sleeps
  2. wait overridden only — real time, plus a heartbeat inside every wait
  3. both overridden — virtual time, so `delay(5000)` costs no real time

(2) and (3) are the same mechanism with different bodies, which is why the
port hands over the wait and not only the clock. Assertions on real
elapsed time are one-sided (`>=` a floor, checked in the sketch) so a
loaded CI machine cannot make them flaky.
"""


def test_clock_hook(dut):
    dut.expect("TEST start clock_hook", timeout=10)

    # --- 1. nothing installed ---------------------------------------

    dut.expect("default: overridden=0", timeout=10)

    # delay(30) really slept, and millis() moved with it.
    dut.expect("real: slept=1 advanced=1", timeout=10)

    # millis() and micros() are the same reading truncated differently.
    dut.expect("agree: 1", timeout=10)

    # --- 2. wait overridden: the heartbeat --------------------------

    dut.expect("tickmode: overridden=1", timeout=10)

    # delay(20) called the hook once per millisecond with a 1000 us
    # slice, still slept for real, and still advanced the real clock.
    dut.expect("tick: count=1 slice=1000 realtime=1 advanced=1", timeout=10)

    # yield() is not a timed wait but goes through the port as a
    # zero-length one, which is where a busy-waiting sketch gives a
    # driver its chance to run.
    dut.expect("yield: ticks_delta=1", timeout=10)

    # --- 3. both overridden: virtual time ---------------------------

    # The whole point: 5000 ms of sketch time, no real time, 5000 one-
    # millisecond slices handed to the driver.
    dut.expect("virtual: advanced=5000 realtime_ms=0 waits=5000", timeout=10)

    # delayMicroseconds is a single wait with no loop, so it lands on the
    # exact amount asked for.
    dut.expect("micros_delay: advanced=1234", timeout=10)

    # The Stream timeouts move with the clock too. readBytes on a stream
    # that never yields a byte burns its full 2000 ms timeout and no real
    # time — this is what stops a UART driver's readBytes from costing
    # wall-clock seconds, and what lets a driver answer from inside it.
    dut.expect("stream: got=0 advanced=2000 realtime_ms=0", timeout=10)

    # Handing the clock back leaves the real one where it always was: the
    # virtual excursion to 5+ seconds did not move it.
    dut.expect("restored: overridden=0 sane=1", timeout=10)

    dut.expect("TEST done", timeout=10)
