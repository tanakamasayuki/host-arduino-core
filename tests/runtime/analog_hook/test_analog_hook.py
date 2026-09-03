"""Tests for the analog / PWM half of the bus observation port (HostBus.h).

Covers what a display library needs for a backlight: `ledcAttach` /
`ledcWrite` / `analogWrite` announced to a hook with the channel,
frequency, resolution and duty the sketch chose; the LEDC refusals that
match silicon; `tone` and `dacWrite` on the same stream; and the read
direction, where `setAnalogValue` / `setAnalogMilliVolts` inject an ADC
reading and an analog read hook can compute one.
"""


def test_analog_hook(dut):
    dut.expect("TEST start analog_hook", timeout=10)

    # Injected readings come back as given. The resolution is recorded,
    # never applied — scaling the value would mean inventing a reference.
    dut.expect("adc: raw=2048 mv=1650 bits=10", timeout=10)
    dut.expect("adc: unset=0", timeout=10)

    # The read hook wins while registered, the injected value comes back
    # once it is gone.
    dut.expect("adc: hooked=1024 restored=2048", timeout=10)

    # The millivolt reading has its own hook and its own injected value.
    # The raw reading is unaffected by it: neither is derived from the
    # other, because there is no attenuation or Vref model to derive with.
    dut.expect("mv: hooked=3300 raw=2048 restored=1650", timeout=10)

    # Width changes are observable in call order. `analogSetWidth` is the
    # legacy spelling of the same knob and reports identically — a trace
    # records the width, not which name set it.
    dut.expect("width: events=2 first=9 last=11 now=10", timeout=10)

    # The backlight: one attach, one duty write, both announced.
    dut.expect("backlight: attach=1 write=1 duty=128 events=2", timeout=10)

    # ledcAttach assigns the lowest free channel, the same one
    # arduino-esp32 would have handed out.
    dut.expect("state: ch=0 f=5000 r=8 d=128 attached=1 chpin=38", timeout=10)
    dut.expect("read: duty=128 freq=5000", timeout=10)

    # LEDC's "full on" fixup: a duty at the maximum is bumped one past it.
    dut.expect("fullon: duty=256", timeout=10)

    dut.expect("retune: freq=1000 r=10 d=256", timeout=10)

    # Everything silicon would refuse is refused, and refusals are silent —
    # a trace never shows work a real board would not have done.
    dut.expect("refused: again=0 wide=0 zero=0 unattached=0 events_delta=0", timeout=10)
    dut.expect("detached: attached=0 chpin=-1", timeout=10)

    # analogWrite on an unattached pin attaches with the global defaults
    # first, so the hook sees two events, not one.
    dut.expect("analogwrite: events_delta=2 f=2000 r=8 d=64", timeout=10)

    # A channel-pinned attach, written through the channel spelling. An
    # unused channel refuses the write.
    dut.expect("channel: pin=4 write=1 free=0 duty=100", timeout=10)

    # A second pin on a used channel adopts that channel's 3000 Hz / 12
    # bits and ignores its own 9999 Hz / 4 bits, and one channel write
    # reaches both pins. ledcChannelPin reports the lower of the two.
    dut.expect("shared: f=3000 r=12 both=1 first=4 a=55 b=55", timeout=10)

    # The pin overload retunes with the *global* resolution, not the pin's:
    # 3000 Hz / 12 bits becomes 1500 Hz / 8 bits. Faithful to
    # arduino-esp32, and the reason a driver that means to keep its own
    # resolution should call ledcChangeFrequency instead.
    dut.expect("retune_global: f=1500 r=8 wbits=8", timeout=10)

    # A tone parks a 50% square wave (0x1FF) at 10 bits, and NOTE_A octave
    # 4 resolves to 440 Hz the way arduino-esp32 resolves it.
    dut.expect("tone: freq=440 note=440 r=10 d=511", timeout=10)

    # A DAC pin shares the per-pin slot with no channel and no frequency.
    dut.expect("dac: write=1 d=200 r=8 ch=255 f=0", timeout=10)
    dut.expect("dac: disable=1 attached=0 again=0", timeout=10)

    # ledcDetach does not claim a DAC pin, but ledcAttach repurposes one —
    # arduino-esp32 releases the old peripheral rather than refusing.
    dut.expect("repurpose: detach=0 attach=1 ch=1 f=1000", timeout=10)

    # The whole recorded stream, in order. This is the shape a test
    # compares against a golden sequence.
    dut.expect("log=20", timeout=10)
    for line in (
        "0 attach pin=38 ch=0 f=5000 r=8 d=0",
        "1 write pin=38 ch=0 f=5000 r=8 d=128",
        "2 write pin=38 ch=0 f=5000 r=8 d=256",
        "3 config pin=38 ch=0 f=1000 r=10 d=256",
        "4 detach pin=38 ch=255 f=1000 r=10 d=0",
        "5 attach pin=38 ch=0 f=2000 r=8 d=0",
        "6 write pin=38 ch=0 f=2000 r=8 d=64",
        "7 attach pin=4 ch=7 f=3000 r=12 d=0",
        "8 write pin=4 ch=7 f=3000 r=12 d=100",
        "9 attach pin=6 ch=7 f=3000 r=12 d=0",
        "10 write pin=4 ch=7 f=3000 r=12 d=55",
        "11 write pin=6 ch=7 f=3000 r=12 d=55",
        "12 config pin=6 ch=7 f=1500 r=8 d=55",
        "13 tone pin=4 ch=7 f=440 r=10 d=511",
        "14 tone pin=4 ch=7 f=440 r=10 d=511",
        "15 detach pin=4 ch=255 f=440 r=10 d=0",
        "16 dac pin=25 ch=255 f=0 r=8 d=200",
        "17 detach pin=25 ch=255 f=0 r=8 d=0",
        "18 dac pin=25 ch=255 f=0 r=8 d=100",
        "19 attach pin=25 ch=1 f=1000 r=8 d=100",
    ):
        dut.expect(line, timeout=10)

    # Unregistering stops the notifications and leaves the state alone.
    dut.expect("cleared: events_delta=0 duty=200", timeout=10)

    # The reset puts pins, channels, injected readings and the
    # analogWrite defaults (12-bit ADC, 1000 Hz, 8-bit duty) back.
    dut.expect("reset: attached=0 chpin=-1 adc=0 bits=12 whz=1000 wbits=8", timeout=10)

    dut.expect("TEST done", timeout=10)
