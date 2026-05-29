"""Headless M5GFX rendering smoke: main panel (drawMain) + sprite cases (drawHome)."""

from pathlib import Path

SKETCH_DIR = Path(__file__).parent
CASES = ["stickcplus_p", "stickcplus_l", "core2", "atoms3", "coreink"]


def test_m5gfx_smoke(dut):
    dut.expect("TEST start m5gfx_smoke", timeout=60)
    dut.expect("MAIN ok", timeout=60)
    for name in CASES:
        dut.expect(f"CASE home_{name}", timeout=60)
    dut.expect("TEST done", timeout=60)

    out = SKETCH_DIR / "output"
    assert (out / "main.png").stat().st_size > 100
    for name in CASES:
        path = out / f"home_{name}.png"
        assert path.exists(), f"missing {path}"
        assert path.stat().st_size > 100
