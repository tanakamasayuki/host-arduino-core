import re


def expect_status(dut, name):
    match = dut.expect(re.compile((rf"{name}=(ok|fail)").encode()), timeout=180)
    assert match.group(1) == b"ok", f"{name}=fail"


def test_esp_timer(dut):
    dut.expect("TEST start esp_timer", timeout=180)
    expect_status(dut, "monotonic")
    expect_status(dut, "delay_lower")
    expect_status(dut, "delay_upper")
    expect_status(dut, "create")
    fired = dut.expect(re.compile(rb"fired=(\d+)"), timeout=180)
    expect_status(dut, "fired_lower")
    expect_status(dut, "fired_upper")
    after_stop = dut.expect(re.compile(rb"after_stop_count=(\d+)"), timeout=180)
    expect_status(dut, "after_stop")
    dut.expect("TEST done", timeout=180)
    assert int(fired.group(1)) >= 4
    assert int(after_stop.group(1)) <= 1
