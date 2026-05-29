def test_freertos_queue(dut):
    dut.expect("TEST start freertos_queue", timeout=60)
    dut.expect("create=ok", timeout=60)
    dut.expect("count=5", timeout=60)
    dut.expect("sum=15", timeout=60)
    dut.expect("timeout=ok", timeout=60)
    dut.expect("TEST done", timeout=60)
