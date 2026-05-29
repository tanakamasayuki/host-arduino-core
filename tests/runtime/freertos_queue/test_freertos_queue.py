def test_freertos_queue(dut):
    dut.expect("TEST start freertos_queue", timeout=180)
    dut.expect("create=ok", timeout=180)
    dut.expect("count=5", timeout=180)
    dut.expect("sum=15", timeout=180)
    dut.expect("timeout=ok", timeout=180)
    dut.expect("TEST done", timeout=180)
