"""Tests for Print / Serial formatting (integer, hex, bin, float, String)."""


def test_print_api(dut):
    dut.expect("TEST start print", timeout=30)
    dut.expect("int:42", timeout=30)
    dut.expect("neg:-7", timeout=30)
    dut.expect("hex:AB", timeout=30)
    dut.expect("bin:1011", timeout=30)
    dut.expect("float:3.14", timeout=30)
    dut.expect("cstr:hello", timeout=30)
    dut.expect("string:world", timeout=30)
    dut.expect("bool:1", timeout=30)
    dut.expect("TEST done", timeout=30)
