def test_freertos_task(dut):
    dut.expect("TEST start freertos_task", timeout=30)
    dut.expect("create=ok", timeout=30)
    dut.expect("handle=ok", timeout=30)
    dut.expect("counter=5", timeout=30)
    dut.expect("done=1", timeout=30)
    dut.expect("TEST done", timeout=30)
