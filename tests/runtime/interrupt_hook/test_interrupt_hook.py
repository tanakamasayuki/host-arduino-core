"""Tests for the interrupt port (cores/host/HostInterrupt.h).

The core keeps what `attachInterrupt` registered and calls it when asked,
and decides nothing else. The negative assertion carries as much weight as
the positive one: moving the pin fires nothing on its own, so there is
exactly one thing deciding when an ISR runs and it is not the core.

`kInterruptEnter` / `kInterruptExit` bracket the handler, which is what
makes an ISR's own bus traffic identifiable as such in a trace — the
`gpio.write pin=2` line below sits inside the brackets.
"""

import re

GOLDEN = [
    # A registration, with the raw mode and the normalized trigger.
    "attach pin=27 mode=2 trig=falling arg=0",
    # Moving the line is announced as a pin write and nothing more. No
    # enter/exit here: the core did not decide an edge had happened.
    "gpio.write pin=27 value=0",
    "gpio.write pin=27 value=1",
    # Now the driver decided, and called. The handler's own write to the
    # LED is bracketed by the two interrupt events.
    "enter pin=27 depth=1 fires=1",
    "gpio.write pin=2 value=1",
    "exit pin=27 depth=0",
    # attachInterruptArg records the arg-taking shape instead.
    "attach pin=33 mode=3 trig=rising arg=1",
    "enter pin=33 depth=1 fires=1",
    "exit pin=33 depth=0",
    "enter pin=33 depth=1 fires=2",
    "exit pin=33 depth=0",
    # Re-arming replaces the handler and keeps the fire count.
    "attach pin=33 mode=1 trig=change arg=0",
    # A handler detaching itself mid-call: the detach lands between the
    # brackets and the exit still happens.
    "enter pin=33 depth=1 fires=3",
    "detach pin=33",
    "exit pin=33 depth=0",
    # An unrecognized mode is reported as unknown, not guessed at.
    "attach pin=33 mode=127 trig=unknown arg=0",
    "detach pin=33",
]


def test_interrupt_hook(dut):
    dut.expect("TEST start interrupt_hook", timeout=10)

    # A sketch that never attaches is where it always was: nothing
    # registered, and the port refuses to invoke anything.
    dut.expect("unattached: attached=0 trig=none fired=0", timeout=10)

    # FALLING is 2 in this core. The normalized trigger is what a driver
    # matches on, because the raw numbers do not agree with arduino-esp32:
    # RISING is 3 here and 1 there, CHANGE is 1 here and 3 there.
    dut.expect("attached: pin=27 mode=2 trig=falling handler=1 arg=0", timeout=10)
    dut.expect("modes: change=change falling=falling rising=rising low=low high=high", timeout=10)

    # The division of labour, stated as an assertion: two pin writes were
    # observed and zero interrupts fired.
    dut.expect("nofire: button_fires=0 slot_fires=0 trace_delta=2", timeout=10)

    # And when the driver matches the trigger itself and calls the port,
    # the handler runs and its side effects land.
    dut.expect("fired: matched=1 fired=1 button_fires=1 led=1", timeout=10)

    # The arg-taking spelling reaches the handler with its argument.
    dut.expect("arg: spare_fires=2 count=20 slot_fires=2", timeout=10)
    dut.expect("rearm: trig=change handler=1 arg=0 fires=2", timeout=10)

    # A handler is allowed to detach itself: the pointer is taken before
    # the call, so the first invocation completes and the second finds
    # nothing.
    dut.expect("selfdetach: first=1 second=0 attached=0", timeout=10)
    dut.expect("unknown: mode=127 trig=unknown", timeout=10)

    dut.expect(f"trace={len(GOLDEN)} overflow=0", timeout=10)
    for line in GOLDEN:
        dut.expect(re.escape(line), timeout=10)

    # Releasing the observation slot stops the notifications and leaves the
    # registration alone — the handler still ran, taking button_fires to 2.
    dut.expect("cleared: delta=0 button_fires=2", timeout=10)

    # A reset detaches everything and zeroes the counters.
    dut.expect("reset: attached=0 fires=0 fired=0", timeout=10)

    dut.expect("TEST done", timeout=10)
