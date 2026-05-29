def test_freertos_mutex(dut):
    dut.expect("TEST start freertos_mutex", timeout=10)
    dut.expect("mutex_create=ok", timeout=10)
    dut.expect("shared=3000", timeout=10)
    dut.expect("bin_empty=ok", timeout=10)
    dut.expect("bin_after_give=ok", timeout=10)
    dut.expect("cnt_taken=3", timeout=10)
    dut.expect("TEST done", timeout=10)
