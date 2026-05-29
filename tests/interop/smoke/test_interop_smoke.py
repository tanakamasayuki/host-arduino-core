"""interop/smoke — proves the sketch compiles and prints over Serial on
both the host and ESP32 profiles.
"""

import re


def test_smoke(dut):
    # The 5s in-sketch delay on top of compile/upload overhead means
    # the first line can take ~10s to appear on ESP32.
    dut.expect(re.compile(rb"INTEROP_SMOKE_READY"), timeout=20)
    dut.expect(re.compile(rb"millis="), timeout=30)
