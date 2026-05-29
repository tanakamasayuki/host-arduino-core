"""Smoke test — verifies the build and harness wiring."""


def test_smoke(dut):
    dut.expect("SMOKE ready", timeout=60)
    dut.expect("millis=", timeout=60)
