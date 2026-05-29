def test_freertos_mutex(dut):
    dut.expect("TEST start freertos_mutex", timeout=30)
    dut.expect("mutex_create=ok", timeout=30)
    dut.expect("shared=3000", timeout=30)
    dut.expect("bin_empty=ok", timeout=30)
    dut.expect("bin_after_give=ok", timeout=30)
    dut.expect("cnt_taken=3", timeout=30)
    dut.expect("TEST done", timeout=30)
