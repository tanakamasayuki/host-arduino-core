"""Tests for the I2C half of the bus observation port (bundled Wire library).

Covers the default "init succeeds, no device present" shape, the
transaction-level write / read hooks a device model registers, an address
scan finding exactly the modelled device, transmit-buffer overflow, the
lifecycle hook that puts `begin()` / `end()` / the configuration setters
into an ordered trace, and unregistering.
"""


def test_wire_hook(dut):
    dut.expect("TEST start wire_hook", timeout=10)

    dut.expect("begin=1 sda=21 scl=22 clock=400000", timeout=10)

    # No hook: the bus initializes but nothing answers — status 2 is an
    # address NACK, and a request yields no bytes.
    dut.expect("nodevice: status=2 read=0 available=0", timeout=10)

    # The write transaction reaches the model whole: address, payload,
    # stop condition.
    dut.expect("write: status=0 len=1 stop=1 pointer=1", timeout=10)

    # And the model answers the read from the register it was pointed at.
    dut.expect("read: got=2 available=2", timeout=10)
    dut.expect("bytes=A1,A2", timeout=10)
    dut.expect("drained: available=0 read=-1", timeout=10)

    dut.expect("poke: status=0 len=2", timeout=10)
    dut.expect("poked: value=5A", timeout=10)

    # A scan sees one device: 0x68 ACKs, the other 118 addresses NACK.
    dut.expect("scan: nack=2 found=1", timeout=10)

    # Overflow is reported as status 1 without reaching the model.
    dut.expect("overflow: accepted=128 status=1", timeout=10)

    # 3 explicit endTransmission calls + 119 scan addresses + the
    # overflowed one; 2 requestFrom calls.
    dut.expect("counts: writes=123 reads=2", timeout=10)

    dut.expect("cleared: status=2 read=0", timeout=10)

    # clearHooks() released the lifecycle slot along with the transaction
    # ones, so the setClock right after it was not reported.
    dut.expect("lifecycle: cleared_delta=0", timeout=10)

    # Re-registered, the setters and end() land on the stream begin() did,
    # in call order — the sequence a golden trace compares against.
    dut.expect("lifecycle: begin,pins,timeout,end", timeout=10)
    dut.expect("teardown: begun=0 sda=1 scl=2 clock=100000 timeout=25", timeout=10)

    dut.expect("wire1: begin=1 bus=1", timeout=10)

    dut.expect("TEST done", timeout=10)
