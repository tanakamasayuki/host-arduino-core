def test_esp_random(dut):
    dut.expect("TEST start esp_random", timeout=180)
    dut.expect("distinct=ok", timeout=180)
    dut.expect("fill_nonzero=ok", timeout=180)
    dut.expect("small_nonzero=ok", timeout=180)
    dut.expect("TEST done", timeout=180)
