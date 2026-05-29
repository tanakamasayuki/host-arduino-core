def test_freertos_queue(dut):
    dut.expect("TEST start freertos_queue", timeout=30)
    dut.expect("create=ok", timeout=30)
    dut.expect("count=5", timeout=30)
    dut.expect("sum=15", timeout=30)
    dut.expect("timeout=ok", timeout=30)
    dut.expect("TEST done", timeout=30)
