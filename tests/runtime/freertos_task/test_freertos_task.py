def test_freertos_task(dut):
    dut.expect("TEST start freertos_task", timeout=10)
    dut.expect("create=ok", timeout=5)
    dut.expect("handle=ok", timeout=5)
    dut.expect("counter=5", timeout=10)
    dut.expect("done=1", timeout=5)
    dut.expect("TEST done", timeout=5)
