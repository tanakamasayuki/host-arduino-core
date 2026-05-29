def test_esp_log(dut):
    dut.expect("TEST start esp_log", timeout=10)
    dut.expect("CORE_DEBUG_LEVEL=0", timeout=10)
    dut.expect("ESP_LOG_NONE=0", timeout=10)
    dut.expect("ESP_LOG_VERBOSE=5", timeout=10)
    dut.expect("BEFORE", timeout=10)
    # No log lines should appear between BEFORE and AFTER — esp_log_level_get
    # returns NONE regardless of the level_set call.
    dut.expect("level_get=0", timeout=10)
    dut.expect("AFTER", timeout=10)
    dut.expect("TEST done", timeout=10)
