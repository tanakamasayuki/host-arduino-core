"""Tests for Print / Serial formatting (integer, hex, bin, float, String)."""


def test_print_api(dut):
    dut.expect("TEST start print", timeout=60)
    dut.expect("int:42", timeout=60)
    dut.expect("neg:-7", timeout=60)
    dut.expect("hex:AB", timeout=60)
    dut.expect("bin:1011", timeout=60)
    dut.expect("float:3.14", timeout=60)
    dut.expect("cstr:hello", timeout=60)
    dut.expect("string:world", timeout=60)
    dut.expect("bool:1", timeout=60)
    dut.expect("TEST done", timeout=60)
