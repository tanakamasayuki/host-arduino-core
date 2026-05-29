def test_freertos_notify(dut):
    dut.expect("TEST start freertos_notify", timeout=10)
    dut.expect("received=3", timeout=10)
    dut.expect("done=1", timeout=10)
    dut.expect("TEST done", timeout=10)
