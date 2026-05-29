"""Headless LovyanGFX rendering smoke: main panel (drawMain) + sprite cases (drawHome)."""

from pathlib import Path

SKETCH_DIR = Path(__file__).parent
CASES = ["stickcplus_p", "stickcplus_l", "core2", "atoms3", "coreink"]


def test_lovyangfx_smoke(dut):
    dut.expect("TEST start lovyangfx_smoke", timeout=120)
    dut.expect("MAIN ok", timeout=120)
    for name in CASES:
        dut.expect(f"CASE home_{name}", timeout=120)
    dut.expect("TEST done", timeout=120)

    out = SKETCH_DIR / "output"
    assert (out / "main.png").stat().st_size > 100
    for name in CASES:
        path = out / f"home_{name}.png"
        assert path.exists(), f"missing {path}"
        assert path.stat().st_size > 100
