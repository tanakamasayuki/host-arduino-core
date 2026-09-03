"""Tests for millis / micros / delay / delayMicroseconds.

The sketch names each failed check and reports whether the runtime was
shutting down, and both are folded into the assertion message. These
bounds are the ones most likely to fail on one CI runner and nowhere
else, and a bare count cannot tell an over-tight upper bound from a
`delay` that returned early because the TCP connection dropped —
`delay()` gives up as soon as the runtime is stopping.
"""

import re


def test_timing(dut):
    dut.expect("TEST start timing", timeout=10)

    stopping = dut.expect(re.compile(rb"stopping=(\d)"), timeout=10).group(1)
    failed = dut.expect(re.compile(rb"failed=(\S+)"), timeout=10).group(1).decode()

    match = dut.expect(re.compile(rb"TEST done (\d+)/(\d+)"), timeout=10)
    passed, total = int(match.group(1)), int(match.group(2))

    assert total > 0, "no assertions ran"
    assert stopping == b"0", (
        "the runtime was already shutting down, so delay() returned "
        f"immediately and the timing bounds mean nothing (failed: {failed})"
    )
    assert passed == total, f"{total - passed} of {total} assertions failed: {failed}"
