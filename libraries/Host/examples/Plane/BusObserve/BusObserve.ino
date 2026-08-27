// BusObserve — watching a bus from the sketch side.
//
// The host core models no peripherals: the code that knows a device's
// protocol is the library that drives it. What the core offers instead is
// an observation port, so a library (or, here, the sketch) can keep its own
// device model and drive it from what goes onto the bus.
//
// This example puts the same tiny ST7789-flavored model behind two
// transports and prints what it decoded:
//
//   1. bit-banged soft SPI — only digitalWrite / digitalRead, the path a
//      board without a hardware SPI peripheral has to take
//   2. the SPI class — one hook per transferred byte, plus the settings
//
// Run it with:  arduino-cli compile -b lang-ship:host:host . && arduino-cli upload -b lang-ship:host:host .

#include <Arduino.h>
#include <SPI.h>

namespace {

constexpr int PIN_SCK = 18;
constexpr int PIN_MISO = 19;
constexpr int PIN_MOSI = 23;
constexpr int PIN_DC = 5;
constexpr int PIN_CS = 15;

// Stand-in for the panel model a display library would own. It knows the
// protocol (DC low means command); the core knows only the pins.
struct PanelModel {
    uint8_t bits = 0;
    uint8_t acc = 0;
    uint8_t command = 0;
    uint16_t x = 0;
    uint16_t y = 0;
    uint8_t args = 0;

    void feedByte(uint8_t byte, bool is_command)
    {
        if (is_command) {
            command = byte;
            args = 0;
            Serial.printf("  command %02X\n", byte);
            return;
        }
        switch (command) {
        case 0x2A: // column address set
            if (args == 0) {
                x = static_cast<uint16_t>(byte << 8);
            } else if (args == 1) {
                x = static_cast<uint16_t>(x | byte);
                Serial.printf("  column start = %u\n", x);
            }
            break;
        case 0x2B: // row address set
            if (args == 0) {
                y = static_cast<uint16_t>(byte << 8);
            } else if (args == 1) {
                y = static_cast<uint16_t>(y | byte);
                Serial.printf("  row start = %u\n", y);
            }
            break;
        default:
            Serial.printf("  data %02X\n", byte);
            break;
        }
        ++args;
    }

    // Soft SPI: called for every pin write, samples MOSI on rising SCK.
    void shiftBit(uint8_t level, bool is_command)
    {
        acc = static_cast<uint8_t>((acc << 1) | (level ? 1 : 0));
        if (++bits == 8) {
            bits = 0;
            const uint8_t byte = acc;
            acc = 0;
            feedByte(byte, is_command);
        }
    }
};

PanelModel model;

void onPinWrite(uint8_t pin, uint8_t value, void *user)
{
    if (pin != PIN_SCK || value == 0) {
        return; // one sample per rising clock edge
    }
    // digitalRead returns the level the sketch last wrote, which is what
    // lets the model read MOSI and DC without the core knowing about
    // either of them.
    static_cast<PanelModel *>(user)->shiftBit(digitalRead(PIN_MOSI), digitalRead(PIN_DC) == LOW);
}

uint8_t onSpiByte(uint8_t out, void *user)
{
    static_cast<PanelModel *>(user)->feedByte(out, digitalRead(PIN_DC) == LOW);
    return 0xFF; // a display is write-only; an SD model would answer here
}

void onSpiTransaction(bool active, const SPISettings &settings, void *user)
{
    (void)user;
    if (active) {
        Serial.printf("  transaction: %u Hz, bit order %u, mode %u\n", settings._clock, settings._bitOrder,
                      settings._dataMode);
    }
}

void softSpiWrite(uint8_t value)
{
    for (int i = 7; i >= 0; --i) {
        digitalWrite(PIN_MOSI, (value >> i) & 1);
        digitalWrite(PIN_SCK, HIGH);
        digitalWrite(PIN_SCK, LOW);
    }
}

void softCommand(uint8_t value)
{
    digitalWrite(PIN_DC, LOW);
    softSpiWrite(value);
}

void softData(uint8_t value)
{
    digitalWrite(PIN_DC, HIGH);
    softSpiWrite(value);
}

} // namespace

void setup()
{
    Serial.begin(115200);
    Serial.println("BusObserve");

    pinMode(PIN_SCK, OUTPUT);
    pinMode(PIN_MOSI, OUTPUT);
    pinMode(PIN_DC, OUTPUT);
    pinMode(PIN_CS, OUTPUT);
    digitalWrite(PIN_CS, HIGH);

    // --- 1. bit-banged soft SPI ------------------------------------
    Serial.println("soft SPI (digitalWrite only):");
    HostArduino::setPinWriteHook(onPinWrite, &model);

    digitalWrite(PIN_CS, LOW);
    softCommand(0x2A);
    softData(0x00);
    softData(0x28);
    digitalWrite(PIN_CS, HIGH);

    HostArduino::clearPinHooks();

    // --- 2. the SPI class ------------------------------------------
    Serial.println("hardware SPI (SPI class):");
    SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
    SPI.setTransferHook(onSpiByte, &model);
    SPI.setTransactionHook(onSpiTransaction);

    SPI.beginTransaction(SPISettings(24000000, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_CS, LOW);
    digitalWrite(PIN_DC, LOW);
    SPI.transfer(0x2B);
    digitalWrite(PIN_DC, HIGH);
    SPI.transfer(0x00);
    SPI.transfer(0x50);
    digitalWrite(PIN_CS, HIGH);
    SPI.endTransaction();

    Serial.printf("bytes transferred: %u\n", SPI.transferCount());
    Serial.println("BusObserve done");
}

void loop()
{
    delay(100);
}
