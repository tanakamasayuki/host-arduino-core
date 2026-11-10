"""ArduinoCore-API / AVR surface on the host core.

Half of this test is that the sketch builds at all: it folds EmbedBench's
-D probe table (docs/HOST_CORE.ja.md §3.5) into one file, and every probe
there failed to build on 1.7.1 except the slave one, which built and
started a master. The other half checks what the additions do.
"""


def test_api_compat(dut):
    dut.expect("TEST start api_compat", timeout=10)

    # Recorded, not enforced: the flag follows the last call.
    dut.expect("interrupts: masked=1 unmasked=1", timeout=10)

    # The four call shapes drivers use all resolve, and all read 6 bytes.
    dut.expect("requestFrom: size=6 int=6 u8=6 two=6", timeout=10)
    # 300 used to be narrowed to a byte (44). It is clamped to the buffer.
    dut.expect("requestFrom: big=128 buffer=128", timeout=10)
    dut.expect("wire: interfaces=2 buf64=128 buf256=0", timeout=10)
    # A call through `arduino::HardwareI2C &` reaches TwoWire.
    dut.expect("hwi2c: read=3", timeout=10)

    # One-argument begin is the slave role (arduino-esp32's meaning). It
    # used to bind to begin(sda) and start a master with sda=8.
    dut.expect_exact("[HostCore] Wire.begin(address): slave mode recorded", timeout=10)
    dut.expect("slave: ok=1 slave=1 addr=8", timeout=10)
    dut.expect("master: slave=0 sda=-1 scl=-1", timeout=10)
    dut.expect("master_pins: slave=0 sda=21 scl=22", timeout=10)

    # setWireTimeout maps onto setTimeOut; a model answering 5 raises the
    # flag, and nothing else does.
    dut.expect("timeout: ms=25 reset=1 flag=0", timeout=10)
    dut.expect("timeout: status=5 flag=1 cleared=0", timeout=10)
    dut.expect("timeout: rounded_ms=2", timeout=10)

    # setPins stores the pins for the following begin().
    dut.expect("spi: sck=1 miso=2 mosi=3 ss=-1 interfaces=1", timeout=10)
    dut.expect("spi: div4=00241001", timeout=10)
    # A call through `arduino::HardwareSPI &` reaches the transfer hook.
    dut.expect("hwspi: out=a5", timeout=10)
    dut.expect("settings: clock=1000000 order=1 mode=0 eq=1", timeout=10)

    # F_CPU is the classic ESP32's 240 MHz; the conversions follow it.
    dut.expect("cpu: f=240000000 cpm=240 us2c=480 c2us=2 noint=-1", timeout=10)
    dut.expect("enums: order=1 lsb=0 mode=3", timeout=10)
    dut.expect("param: seen=42", timeout=10)

    # Servo: 90 degrees over 544..2400 us is 1472 us; 3000 clamps to 2400.
    # Each step reaches the analog write hook as LEDC at 50 Hz / 16 bits,
    # duty = us * 65536 / 20000.
    dut.expect("servo: ch=0 angle=90 us=1472 clamped=2400 attached=0 back=1472", timeout=10)
    dut.expect(
        "servo_trace: attach:50/16/0,write:50/16/4823,write:50/16/7864,detach:50/16/0",
        timeout=10,
    )

    dut.expect("TEST done", timeout=10)
