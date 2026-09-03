"""Acceptance rehearsal: a virtual-clock tick model running on the three
extension ports at once (clock + lifecycle + device UART + GPIO
injection). The sketch's application half is an ordinary debounce +
AT-command app that knows nothing about virtual time.

Where the focused tests each check one port in isolation
(`clock_hook`, `lifecycle_hook`, `uart_buffer`, `gpio_hook`), this checks
that a harness can drive all of them together from underneath a sketch
that was not written for it. Every figure below is exact rather than a
range: the clock is virtual, so nothing here depends on machine load.

The middle layer installs itself from a global constructor, so a virtual
clock is already in force when `main()` starts the runtime. This test
reaching Serial at all is therefore also evidence that the runtime's own
socket and startup waits stayed on real time — see "Timeouts that stay on
real time" in README.md.
"""


def test_accept_emu_tick(dut):
    dut.expect("TEST start accept_emu_tick", timeout=10)
    dut.expect("overridden=1", timeout=10)

    # Both once-only lifecycle points were seen, which needs the hook
    # installed before main() — from setup() kPreSetup is already gone.
    dut.expect("phases: preSetup=1 postSetup=1", timeout=10)

    # A request written and answered within one loop() iteration, which
    # only works because Stream's wait goes through the clock port.
    dut.expect("at_reply_ok=1", timeout=10)
    # Exactly one 1 ms slice of that wait was enough for the director to
    # answer, ...
    dut.expect("at_virtual_ms=1", timeout=10)
    # ... and independently of the slice size, the reply landed nowhere
    # near the 100 ms timeout. Keeps a future change in slice size from
    # silently turning this into a near-timeout pass.
    dut.expect("at_fast=1", timeout=10)

    # The button press assembled from raw GPIO injections, detected by the
    # app's own debounce: held for the 20 virtual ms the director scripted.
    dut.expect("PRESS held_ms=20", timeout=10)

    # delay(5000) cost 5000 virtual ms and almost no real time.
    dut.expect("delay_virtual_ms=5000", timeout=10)
    dut.expect("delay_real_fast=1", timeout=10)
    # And the director ran on every one of those 5000 slices. This is what
    # handing over the wait buys over handing over only the clock: without
    # it the delay would be 5 dead seconds in which nothing outside the
    # sketch could happen.
    dut.expect("director_in_delay=5000", timeout=10)

    dut.expect("presses=1", timeout=10)
    dut.expect("TEST done", timeout=10)
