"""Tests for the SPI half of the bus observation port (bundled SPI library).

Covers the transfer hook (bytes out, MISO answer back), the transaction
hook exposing SPISettings, the recorded pins / byte count, and the
no-device default of an idle bus reading 0xFF.
"""


def test_spi_hook(dut):
    dut.expect("TEST start spi_hook", timeout=10)

    dut.expect("pins: sck=18 miso=19 mosi=23 ss=5 begun=1", timeout=10)

    # Nothing attached: the bus reads as idle, but the byte still counts.
    dut.expect("nohook: read=FF count=1", timeout=10)

    # SPISettings reaches the hook exactly as the sketch passed it
    # (MSBFIRST is 1, SPI_MODE0 is 0).
    dut.expect("settings: clock=24000000 order=1 mode=0 active=1 in=1", timeout=10)

    # The accessors and the arduino-esp32 `_clock` / `_bitOrder` /
    # `_dataMode` field spelling report the same values.
    dut.expect("fields: agree=1", timeout=10)

    # The model's answer is what the sketch reads back (~0x2A == 0xD5).
    dut.expect("miso: sent=2A read=D5", timeout=10)

    # transfer16 splits high byte first while MSBFIRST is in force:
    # ~0xBE == 0x41, ~0xEF == 0x10.
    dut.expect("wide: read=4110", timeout=10)

    dut.expect("buffer: FE,FD,FC", timeout=10)
    dut.expect("end: active=0 in=0 transactions=1", timeout=10)

    # Every byte of every spelling reached the model, in order.
    dut.expect("seen=2A,BE,EF,01,02,03,F8,00", timeout=10)
    dut.expect("count=8", timeout=10)

    dut.expect("second: clock=1000000 order=0 mode=3", timeout=10)
    dut.expect("cleared: read=FF", timeout=10)
    dut.expect("ended: begun=0", timeout=10)

    dut.expect("TEST done", timeout=10)
