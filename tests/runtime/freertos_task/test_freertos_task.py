def test_freertos_task(dut):
    dut.expect("TEST start freertos_task", timeout=60)
    dut.expect("create=ok", timeout=60)
    dut.expect("handle=ok", timeout=60)
    dut.expect("counter=5", timeout=60)
    dut.expect("done=1", timeout=60)
    dut.expect("TEST done", timeout=60)
