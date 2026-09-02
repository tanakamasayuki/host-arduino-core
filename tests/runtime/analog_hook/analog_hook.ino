// Tests for the analog / PWM half of the bus observation port
// (cores/host/HostBus.h).
//
// The shape being verified is the one a display library needs for its
// backlight: `ledcAttach` / `analogWrite` / `ledcWrite` announce what
// happened to the pin, in order, with the frequency, resolution, channel,
// and duty the sketch chose. Nothing is emitted on the pin — this half of
// the port records configuration and duty, it does not synthesize a
// waveform — so what a test asserts is the sequence and the values.
//
// The read direction is here too: `HostArduino::setAnalogValue` injects an
// ADC reading and `setAnalogMilliVolts` injects the millivolt one, the
// analog counterparts of `setPinValue`.

#include <Arduino.h>

namespace {

constexpr int PIN_BACKLIGHT = 38;
constexpr int PIN_BUZZER = 4;
constexpr int PIN_SHARED = 6;
constexpr int PIN_DAC = 25;
constexpr int PIN_BATTERY = 34;

// Stand-in for what a display library would own: it does not care how the
// duty is produced, only that the panel ends up lit at the brightness the
// sketch asked for. Recording the event stream is what lets a test compare
// an init sequence against a golden one.
struct BacklightModel {
    char log[32][40] = {{0}};
    uint8_t count = 0;
    uint32_t duty = 0;

    void note(const char *what, const HostArduino::AnalogOut &out)
    {
        if (count >= 32) {
            return;
        }
        snprintf(log[count], sizeof(log[0]), "%s pin=%u ch=%u f=%u r=%u d=%u", what, out.pin, out.channel,
                 out.frequency, out.resolution, out.duty);
        ++count;
    }
};

BacklightModel model;

const char *eventName(HostArduino::AnalogWriteEvent event)
{
    switch (event) {
    case HostArduino::kAnalogAttach:
        return "attach";
    case HostArduino::kAnalogWrite:
        return "write";
    case HostArduino::kAnalogConfig:
        return "config";
    case HostArduino::kAnalogTone:
        return "tone";
    case HostArduino::kAnalogDetach:
        return "detach";
    case HostArduino::kAnalogDac:
        return "dac";
    }
    return "?";
}

void onAnalogWrite(HostArduino::AnalogWriteEvent event, const HostArduino::AnalogOut &out, void *user)
{
    BacklightModel *m = static_cast<BacklightModel *>(user);
    m->note(eventName(event), out);
    if (event == HostArduino::kAnalogWrite) {
        m->duty = out.duty;
    }
}

// A model that computes an ADC reading at read time instead of pushing one
// with setAnalogValue: a divider that halves whatever is on the pin.
uint16_t onAnalogRead(uint8_t pin, uint16_t held, void *user)
{
    (void)user;
    if (pin == PIN_BATTERY) {
        return static_cast<uint16_t>(held / 2);
    }
    return held;
}

} // namespace

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start analog_hook");

    // --- read direction ----------------------------------------------

    // 10 bits rather than the ESP32 default of 12, so the reset at the
    // end of this sketch has something visible to put back.
    analogReadResolution(10);
    HostArduino::setAnalogValue(PIN_BATTERY, 2048);
    HostArduino::setAnalogMilliVolts(PIN_BATTERY, 1650);
    Serial.printf("adc: raw=%u mv=%u bits=%u\n", analogRead(PIN_BATTERY),
                  analogReadMilliVolts(PIN_BATTERY), HostArduino::analogReadBits());

    // A pin nothing was injected into reads 0, not garbage.
    Serial.printf("adc: unset=%u\n", analogRead(35));

    // The read hook wins over the injected value while it is registered.
    HostArduino::setAnalogReadHook(onAnalogRead);
    const uint16_t hooked = analogRead(PIN_BATTERY);
    HostArduino::setAnalogReadHook(nullptr);
    Serial.printf("adc: hooked=%u restored=%u\n", hooked, analogRead(PIN_BATTERY));

    // --- write direction: the backlight ------------------------------

    HostArduino::setAnalogWriteHook(onAnalogWrite, &model);

    // What a display driver does: claim a channel, then set brightness.
    const bool attached = ledcAttach(PIN_BACKLIGHT, 5000, 8);
    const bool lit = ledcWrite(PIN_BACKLIGHT, 128);
    Serial.printf("backlight: attach=%d write=%d duty=%u events=%u\n", attached ? 1 : 0, lit ? 1 : 0,
                  model.duty, model.count);

    // The accessors report the same thing without a hook. ledcReadFreq is
    // faithful to arduino-esp32 and reads 0 while the duty is 0, so
    // analogOut() is what a test should assert configuration against.
    const HostArduino::AnalogOut &out = HostArduino::analogOut(PIN_BACKLIGHT);
    Serial.printf("state: ch=%u f=%u r=%u d=%u attached=%d chpin=%d\n", out.channel, out.frequency,
                  out.resolution, out.duty, out.attached ? 1 : 0, HostArduino::ledcChannelPin(0));
    Serial.printf("read: duty=%u freq=%u\n", ledcRead(PIN_BACKLIGHT), ledcReadFreq(PIN_BACKLIGHT));

    // Full scale is bumped one past the maximum, the LEDC "full on" fixup.
    ledcWrite(PIN_BACKLIGHT, 255);
    Serial.printf("fullon: duty=%u\n", ledcRead(PIN_BACKLIGHT));

    // Retuning reports a config event and keeps the duty.
    const uint32_t changed = ledcChangeFrequency(PIN_BACKLIGHT, 1000, 10);
    Serial.printf("retune: freq=%u r=%u d=%u\n", changed, out.resolution, out.duty);

    // Refusals match silicon: a second attach of the same pin fails, and a
    // resolution wider than the timer fails. Neither reports an event.
    const uint8_t before_refusals = model.count;
    const bool again = ledcAttach(PIN_BACKLIGHT, 5000, 8);
    const bool too_wide = ledcAttach(39, 5000, HostArduino::kLedcMaxResolution + 1);
    const bool no_freq = ledcAttach(39, 0, 8);
    const bool unattached = ledcWrite(39, 10);
    Serial.printf("refused: again=%d wide=%d zero=%d unattached=%d events_delta=%u\n", again ? 1 : 0,
                  too_wide ? 1 : 0, no_freq ? 1 : 0, unattached ? 1 : 0,
                  model.count - before_refusals);

    // Detaching frees the channel for the next attach.
    ledcDetach(PIN_BACKLIGHT);
    Serial.printf("detached: attached=%d chpin=%d\n", HostArduino::analogOut(PIN_BACKLIGHT).attached ? 1 : 0,
                  HostArduino::ledcChannelPin(0));

    // --- analogWrite, and the channel spelling -----------------------

    // analogWrite attaches on first use with the global defaults, so the
    // hook sees an attach and then a write — the same two steps
    // arduino-esp32 performs.
    analogWriteResolution(8);
    analogWriteFrequency((uint32_t)2000);
    const uint8_t before_analog = model.count;
    analogWrite(PIN_BACKLIGHT, 64);
    Serial.printf("analogwrite: events_delta=%u f=%u r=%u d=%u\n", model.count - before_analog,
                  out.frequency, out.resolution, out.duty);

    // A pin can be pinned to a chosen channel, and written through it.
    ledcAttachChannel(PIN_BUZZER, 3000, 12, 7);
    const bool by_channel = ledcWriteChannel(7, 100);
    const bool free_channel = ledcWriteChannel(9, 100);
    Serial.printf("channel: pin=%d write=%d free=%d duty=%u\n", HostArduino::ledcChannelPin(7),
                  by_channel ? 1 : 0, free_channel ? 1 : 0, ledcRead(PIN_BUZZER));

    // A second pin on a used channel adopts that channel's frequency and
    // resolution — its own arguments are ignored, as on silicon — and a
    // channel write then reaches every pin on the channel, not just the
    // first. Two backlights sharing one channel is the case that makes the
    // difference.
    ledcAttachChannel(PIN_SHARED, 9999, 4, 7);
    const bool both = ledcWriteChannel(7, 55);
    Serial.printf("shared: f=%u r=%u both=%d first=%d a=%u b=%u\n",
                  HostArduino::analogOut(PIN_SHARED).frequency,
                  HostArduino::analogOut(PIN_SHARED).resolution, both ? 1 : 0,
                  HostArduino::ledcChannelPin(7), ledcRead(PIN_BUZZER), ledcRead(PIN_SHARED));

    // The pin overloads retune an attached pin with the *global* other
    // half, not the pin's own — arduino-esp32's behavior even though it can
    // narrow a resolution the sketch chose explicitly. Here 3000 Hz / 12
    // bits becomes 1500 Hz / 8 bits, because 8 is the analogWrite default.
    analogWriteFrequency(PIN_SHARED, 1500);
    Serial.printf("retune_global: f=%u r=%u wbits=%u\n", HostArduino::analogOut(PIN_SHARED).frequency,
                  HostArduino::analogOut(PIN_SHARED).resolution, HostArduino::analogWriteBits());

    // --- tone and DAC ------------------------------------------------

    // A tone parks a 50% square wave at 10-bit resolution, and a note is
    // resolved to a frequency the same way arduino-esp32 resolves it.
    const uint32_t tone_freq = ledcWriteTone(PIN_BUZZER, 440);
    const uint32_t note_freq = ledcWriteNote(PIN_BUZZER, NOTE_A, 4);
    Serial.printf("tone: freq=%u note=%u r=%u d=%u\n", tone_freq, note_freq,
                  HostArduino::analogOut(PIN_BUZZER).resolution, ledcRead(PIN_BUZZER));

    ledcDetach(PIN_BUZZER);
    const bool dac_written = dacWrite(PIN_DAC, 200);
    const HostArduino::AnalogOut &dac = HostArduino::analogOut(PIN_DAC);
    Serial.printf("dac: write=%d d=%u r=%u ch=%u f=%u\n", dac_written ? 1 : 0, dac.duty, dac.resolution,
                  dac.channel, dac.frequency);
    const bool dac_off = dacDisable(PIN_DAC);
    Serial.printf("dac: disable=%d attached=%d again=%d\n", dac_off ? 1 : 0, dac.attached ? 1 : 0,
                  dacDisable(PIN_DAC) ? 1 : 0);

    // A pin last written through dacWrite is repurposed by ledcAttach
    // rather than refused, the way arduino-esp32's perimanClearPinBus
    // releases the old peripheral. ledcDetach, conversely, does not claim
    // to detach a DAC pin.
    dacWrite(PIN_DAC, 100);
    const bool detach_dac = ledcDetach(PIN_DAC);
    const bool repurposed = ledcAttach(PIN_DAC, 1000, 8);
    Serial.printf("repurpose: detach=%d attach=%d ch=%u f=%u\n", detach_dac ? 1 : 0, repurposed ? 1 : 0,
                  HostArduino::analogOut(PIN_DAC).channel, HostArduino::analogOut(PIN_DAC).frequency);

    // --- the recorded sequence ---------------------------------------

    Serial.printf("log=%u\n", model.count);
    for (uint8_t i = 0; i < model.count; ++i) {
        Serial.printf("  %u %s\n", i, model.log[i]);
    }

    // Unregistering stops the notifications but leaves the state.
    HostArduino::clearAnalogHooks();
    const uint8_t before_clear = model.count;
    ledcWrite(PIN_BACKLIGHT, 200);
    Serial.printf("cleared: events_delta=%u duty=%u\n", model.count - before_clear,
                  ledcRead(PIN_BACKLIGHT));

    // And a reset puts every pin, channel, and injected reading back.
    HostArduino::resetAnalogState();
    Serial.printf("reset: attached=%d chpin=%d adc=%u bits=%u whz=%u wbits=%u\n",
                  HostArduino::analogOut(PIN_BACKLIGHT).attached ? 1 : 0, HostArduino::ledcChannelPin(0),
                  analogRead(PIN_BATTERY), HostArduino::analogReadBits(), HostArduino::analogWriteHz(),
                  HostArduino::analogWriteBits());

    Serial.println("TEST done");
}

void loop()
{
    delay(10);
}
