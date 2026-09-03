"""Tests for the lifecycle port (cores/host/HostLifecycle.h).

The four points bracket the Arduino thunk, and the only thing a test
driver can build on is that their position and order never move. So the
whole phase sequence is compared against a golden list.

The order that matters is `kPostLoop` before `runtimePoll()`, which is
what the `loops=` stamps pin down: `kPostLoop` carries the count of the
iteration it closes out, and external input taken in by `runtimePoll()`
therefore always belongs to the next one.
"""

import re

GOLDEN = [
    # Once, after the runtime is up and before setup(). The hook was
    # installed from a global constructor, which is why this is first.
    "preSetup loops=0",
    "setup loops=0",
    "postSetup loops=0",
    # Three complete iterations. postLoop is counted before the hook runs,
    # so it reports the iteration it is closing out, not the next one.
    "preLoop loops=0",
    "loop loops=0",
    "postLoop loops=1",
    "preLoop loops=1",
    "loop loops=1",
    "postLoop loops=2",
    "preLoop loops=2",
    "loop loops=2",
    "postLoop loops=3",
    # And the start of a fourth, which is where the sketch reports.
    "preLoop loops=3",
    "loop loops=3",
]


def test_lifecycle_hook(dut):
    dut.expect("TEST start lifecycle_hook", timeout=10)

    # By the time setup() runs, preSetup has already been announced —
    # two entries, preSetup and setup's own.
    dut.expect("insetup: recorded=2 loops=0", timeout=10)

    dut.expect(f"trace={len(GOLDEN)} overflow=0", timeout=10)

    # Matched literally and in order: dut.expect treats its argument as a
    # regex, and a golden comparison should not.
    for line in GOLDEN:
        dut.expect(re.escape(line), timeout=10)

    # With no hook installed the thunk behaves exactly as it did before
    # this port existed: nothing announced, iterations still counted.
    dut.expect("cleared: delta=0 loops_delta=1", timeout=10)

    dut.expect("TEST done", timeout=10)
