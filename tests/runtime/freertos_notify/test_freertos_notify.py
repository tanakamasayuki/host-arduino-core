def test_freertos_notify(dut):
    dut.expect("TEST start freertos_notify", timeout=60)
    dut.expect("received=3", timeout=60)
    dut.expect("done=1", timeout=60)
    dut.expect("TEST done", timeout=60)
