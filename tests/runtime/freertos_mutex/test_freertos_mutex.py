def test_freertos_mutex(dut):
    dut.expect("TEST start freertos_mutex", timeout=30)
    dut.expect("mutex_create=ok", timeout=5)
    dut.expect("shared=3000", timeout=15)
    dut.expect("bin_empty=ok", timeout=5)
    dut.expect("bin_after_give=ok", timeout=5)
    dut.expect("cnt_taken=3", timeout=5)
    dut.expect("TEST done", timeout=5)
