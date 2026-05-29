import re


def test_preferences(dut):
    dut.expect("TEST start preferences", timeout=30)
    match = dut.expect(re.compile(rb"TEST done (\d+)/(\d+)"), timeout=15)
    passed, total = int(match.group(1)), int(match.group(2))
    assert passed == total, f"{total - passed} of {total} assertions failed"
    assert total > 0
