"""Tests for Print / Serial formatting (integer, hex, bin, float, String)."""


def test_print_api(dut):
    dut.expect("TEST start print", timeout=180)
    dut.expect("int:42", timeout=180)
    dut.expect("neg:-7", timeout=180)
    dut.expect("hex:AB", timeout=180)
    dut.expect("bin:1011", timeout=180)
    dut.expect("float:3.14", timeout=180)
    dut.expect("cstr:hello", timeout=180)
    dut.expect("string:world", timeout=180)
    dut.expect("bool:1", timeout=180)
    dut.expect("TEST done", timeout=180)
