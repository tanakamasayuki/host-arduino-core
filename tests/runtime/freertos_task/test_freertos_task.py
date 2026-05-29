def test_freertos_task(dut):
    dut.expect("TEST start freertos_task", timeout=180)
    dut.expect("create=ok", timeout=180)
    dut.expect("handle=ok", timeout=180)
    dut.expect("counter=5", timeout=180)
    dut.expect("done=1", timeout=180)
    dut.expect("TEST done", timeout=180)
