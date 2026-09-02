"""One golden trace across every half of the bus observation port.

The per-bus hook tests (`gpio_hook`, `spi_hook`, `wire_hook`,
`analog_hook`) each check one bus in isolation. This one checks what none
of them can: that a driver's whole startup lands in a single ordered
sequence — I2C up, reset pulse, SPI up, backlight configured dark,
commands, backlight lit, touch probed — and that the sequence can be
compared against a golden list line for line.

The golden below is the assertion. A step moving relative to its
neighbours fails here even though every end-state assertion still passes,
which is exactly the class of display-driver bug an end-state test misses.
"""

import re

# The order a panel driver's begin() has to produce. Read it as the
# specification: the backlight is configured *before* the init commands
# but only lit *after* them, so a half-initialized panel is never visible.
GOLDEN = [
    # the touch controller shares the board's I2C bus
    "i2c.begin sda=21 scl=22 clock=100000",
    "i2c.clock 400000",
    # hardware reset, bit-banged on a plain GPIO
    "gpio.mode pin=33 mode=1",
    "gpio.write pin=33 value=0",
    "gpio.write pin=33 value=1",
    # the panel's SPI bus, with CS parked high first
    "gpio.mode pin=5 mode=1",
    "gpio.write pin=5 value=1",
    "spi.begin sck=18 mosi=23 cs=5",
    # backlight configured but dark
    "pwm.attach pin=38 ch=0 f=5000 r=8",
    "pwm.write pin=38 duty=0",
    # the init commands, inside one transaction and one CS assertion
    "spi.txn active=1 clock=40000000 mode=0",
    "gpio.write pin=5 value=0",
    "spi.byte 01 cs=0",
    "spi.byte 11 cs=0",
    "gpio.write pin=5 value=1",
    "spi.txn active=0 clock=40000000 mode=0",
    # and only now the backlight comes up
    "pwm.write pin=38 duty=200",
    # the touch controller answers its chip-id register
    "i2c.write addr=38 len=1 stop=1",
    "i2c.read addr=38 len=1 stop=1",
]


def test_bus_trace(dut):
    dut.expect("TEST start bus_trace", timeout=10)

    # The whole startup fits the buffer, and the modelled touch controller
    # answered the chip-id read through the I2C read hook.
    dut.expect(f"trace={len(GOLDEN)} overflow=0 chip=A8", timeout=10)

    # Matched literally and in order: dut.expect treats its argument as a
    # regex, and a golden comparison should not.
    for line in GOLDEN:
        dut.expect(re.escape(line), timeout=10)

    # Releasing every slot leaves an unobserved bus behind — no hook fires
    # for the writes after `clearHooks()`, which is how one test hands the
    # buses over to the next model.
    dut.expect("released: delta=0", timeout=10)

    dut.expect("TEST done", timeout=10)
