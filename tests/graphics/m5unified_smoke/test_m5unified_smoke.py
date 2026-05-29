"""Headless M5Unified rendering smoke: main panel (drawMain) + sprite cases (drawHome)."""

from pathlib import Path

SKETCH_DIR = Path(__file__).parent
CASES = ["stickcplus_p", "stickcplus_l", "core2", "atoms3", "coreink"]


def test_m5unified_smoke(dut):
    dut.expect("TEST start m5unified_smoke", timeout=180)
    dut.expect("MAIN ok", timeout=180)
    for name in CASES:
        dut.expect(f"CASE home_{name}", timeout=180)
    dut.expect("TEST done", timeout=180)

    out = SKETCH_DIR / "output"
    assert (out / "main.png").stat().st_size > 100
    for name in CASES:
        path = out / f"home_{name}.png"
        assert path.exists(), f"missing {path}"
        assert path.stat().st_size > 100
