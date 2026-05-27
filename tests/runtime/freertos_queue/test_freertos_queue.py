def test_freertos_queue(dut):
    dut.expect("TEST start freertos_queue", timeout=10)
    dut.expect("create=ok", timeout=5)
    dut.expect("count=5", timeout=10)
    dut.expect("sum=15", timeout=5)
    dut.expect("timeout=ok", timeout=5)
    dut.expect("TEST done", timeout=5)
