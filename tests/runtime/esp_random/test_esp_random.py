def test_esp_random(dut):
    dut.expect("TEST start esp_random", timeout=10)
    dut.expect("distinct=ok", timeout=5)
    dut.expect("fill_nonzero=ok", timeout=5)
    dut.expect("small_nonzero=ok", timeout=5)
    dut.expect("TEST done", timeout=5)
