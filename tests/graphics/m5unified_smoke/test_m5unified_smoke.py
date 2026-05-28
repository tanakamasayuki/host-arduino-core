"""Headless M5Unified rendering smoke test on host with mode=lgfx."""

import re
from pathlib import Path

SKETCH_DIR = Path(__file__).parent


def test_m5unified_smoke(dut):
    dut.expect("TEST start m5unified_smoke", timeout=15)
    dut.expect(re.compile(rb"size=(\d+)x(\d+)"), timeout=5)
    match = dut.expect(re.compile(rb"CAPTURE bytes=(\d+)"), timeout=15)
    png_len = int(match.group(1))
    assert png_len > 100
    dut.expect("TEST done", timeout=5)

    capture = SKETCH_DIR / "output" / "m5unified_smoke_capture.png"
    assert capture.exists(), "PNG capture not written"
    assert capture.stat().st_size == png_len
