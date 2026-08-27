// Tests for the GPIO half of the bus observation port
// (cores/host/HostBus.h).
//
// The shape being verified is the one a display / IR / soft-I2C library
// needs: the library keeps its own device model, the core only announces
// pin writes and remembers pin levels. Here the "device model" is a
// stand-in ST7789 that reassembles bytes from SCK rising edges and tells
// commands from data by reading the DC line — exactly what a real panel
// model would do, without the core knowing anything about panels.

#include <Arduino.h>

namespace {

constexpr int PIN_SCK = 18;
constexpr int PIN_MOSI = 23;
constexpr int PIN_DC = 5;
constexpr int PIN_CS = 15;
constexpr int PIN_BUSY = 21;

struct PanelModel {
    uint8_t bits = 0;
    uint8_t acc = 0;
    uint32_t writes = 0;
    uint8_t commands[8] = {0};
    uint8_t command_count = 0;
    uint8_t data[16] = {0};
    uint8_t data_count = 0;
};

PanelModel model;

void onPinWrite(uint8_t pin, uint8_t value, void *user)
{
    PanelModel *m = static_cast<PanelModel *>(user);
    ++m->writes;

    if (pin != PIN_SCK || value == 0) {
        return; // sample MOSI on the rising edge only
    }
    m->acc = static_cast<uint8_t>((m->acc << 1) | (digitalRead(PIN_MOSI) ? 1 : 0));
    if (++m->bits < 8) {
        return;
    }

    const bool is_command = digitalRead(PIN_DC) == LOW;
    if (is_command) {
        if (m->command_count < sizeof(m->commands)) {
            m->commands[m->command_count++] = m->acc;
        }
    } else if (m->data_count < sizeof(m->data)) {
        m->data[m->data_count++] = m->acc;
    }
    m->bits = 0;
    m->acc = 0;
}

// A device model that computes an input level at read time instead of
// pushing one with setPinValue: BUSY reads back inverted.
int onPinRead(uint8_t pin, uint8_t held, void *user)
{
    (void)user;
    if (pin == PIN_BUSY) {
        return held ? LOW : HIGH;
    }
    return held;
}

void softSpiWrite(uint8_t value)
{
    for (int i = 7; i >= 0; --i) {
        digitalWrite(PIN_MOSI, (value >> i) & 1);
        digitalWrite(PIN_SCK, HIGH);
        digitalWrite(PIN_SCK, LOW);
    }
}

void writeCommand(uint8_t value)
{
    digitalWrite(PIN_DC, LOW);
    softSpiWrite(value);
}

void writeData(uint8_t value)
{
    digitalWrite(PIN_DC, HIGH);
    softSpiWrite(value);
}

} // namespace

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start gpio_hook");

    pinMode(PIN_SCK, OUTPUT);
    pinMode(PIN_MOSI, OUTPUT);
    pinMode(PIN_DC, OUTPUT);
    pinMode(PIN_CS, OUTPUT);
    pinMode(PIN_BUSY, INPUT_PULLUP);
    Serial.printf("mode: sck=%u dc=%u busy=%u\n", HostArduino::pinModeOf(PIN_SCK),
                  HostArduino::pinModeOf(PIN_DC), HostArduino::pinModeOf(PIN_BUSY));

    // A pulled-up input reads HIGH with nothing driving the line; a
    // pulled-down one reads LOW. This is what lets a released open-drain
    // line (soft I2C letting SDA go) read back correctly.
    Serial.printf("pullup=%d\n", digitalRead(PIN_BUSY));
    pinMode(PIN_BUSY, INPUT_PULLDOWN);
    Serial.printf("pulldown=%d\n", digitalRead(PIN_BUSY));

    // digitalRead hands back the last written level.
    digitalWrite(PIN_CS, LOW);
    const int low = digitalRead(PIN_CS);
    digitalWrite(PIN_CS, HIGH);
    const int high = digitalRead(PIN_CS);
    Serial.printf("readback: low=%d high=%d\n", low, high);

    HostArduino::setPinWriteHook(onPinWrite, &model);

    // Push a frame the way TinyGFXBusSoftSPI would: chip select, one
    // command, two data bytes. 77 writes in total — 2 for CS, 3 for DC,
    // and 8 x (MOSI + two SCK edges) x 3 bytes.
    digitalWrite(PIN_CS, LOW);
    writeCommand(0x2A);
    writeData(0x00);
    writeData(0xEF);
    digitalWrite(PIN_CS, HIGH);

    Serial.print("frame: cmd=");
    for (uint8_t i = 0; i < model.command_count; ++i) {
        Serial.printf("%02X", model.commands[i]);
    }
    Serial.print(" data=");
    for (uint8_t i = 0; i < model.data_count; ++i) {
        Serial.printf("%s%02X", i ? "," : "", model.data[i]);
    }
    Serial.println();
    Serial.printf("writes=%u\n", model.writes);

    // Injected input: the response direction for GPIO.
    HostArduino::setPinValue(PIN_BUSY, HIGH);
    Serial.printf("inject=%d\n", digitalRead(PIN_BUSY));

    // Read hook wins over the stored level while it is registered.
    HostArduino::setPinReadHook(onPinRead);
    const int hooked = digitalRead(PIN_BUSY);
    HostArduino::setPinReadHook(nullptr);
    Serial.printf("readhook: hooked=%d restored=%d\n", hooked, digitalRead(PIN_BUSY));

    // Out-of-range pins are dropped, not reported and not written.
    const uint32_t before_oob = model.writes;
    digitalWrite(HostArduino::kGpioPinCount + 4, HIGH);
    digitalWrite(-1, HIGH);
    Serial.printf("oob: writes_delta=%u read=%d\n", model.writes - before_oob,
                  digitalRead(HostArduino::kGpioPinCount + 4));

    // Unregistering stops the notifications but leaves the pin state.
    HostArduino::clearPinHooks();
    const uint32_t before_clear = model.writes;
    digitalWrite(PIN_SCK, HIGH);
    digitalWrite(PIN_SCK, LOW);
    Serial.printf("cleared: writes_delta=%u value=%d\n", model.writes - before_clear,
                  HostArduino::pinValue(PIN_SCK));

    Serial.println("TEST done");
}

void loop()
{
    delay(10);
}
