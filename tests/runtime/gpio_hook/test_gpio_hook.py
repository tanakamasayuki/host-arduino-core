"""Tests for the GPIO half of the bus observation port (HostBus.h).

Covers what a bit-banging library needs: every `digitalWrite` announced
to a hook, pin levels held for `digitalRead`, pull-up / pull-down seeding,
injected input, a read hook, out-of-range pins dropped, and unregistering.
"""


def test_gpio_hook(dut):
    dut.expect("TEST start gpio_hook", timeout=10)

    # pinMode is recorded: OUTPUT=1, INPUT_PULLUP=2.
    dut.expect("mode: sck=1 dc=1 busy=2", timeout=10)

    # A pulled-up input reads HIGH, a pulled-down one LOW.
    dut.expect("pullup=1", timeout=10)
    dut.expect("pulldown=0", timeout=10)

    # digitalRead returns the last value written.
    dut.expect("readback: low=0 high=1", timeout=10)

    # The sketch-side panel model reassembled the bit-banged frame and
    # classified the bytes by the DC line it read through digitalRead.
    dut.expect("frame: cmd=2A data=00,EF", timeout=10)

    # Every single write is announced: 2 x CS + 3 x DC + 3 bytes x 8 bits
    # x (MOSI + two SCK edges).
    dut.expect("writes=77", timeout=10)

    dut.expect("inject=1", timeout=10)
    dut.expect("readhook: hooked=0 restored=1", timeout=10)
    dut.expect("oob: writes_delta=0 read=0", timeout=10)
    dut.expect("cleared: writes_delta=0 value=0", timeout=10)

    dut.expect("TEST done", timeout=10)
