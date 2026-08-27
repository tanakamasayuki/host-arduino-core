// Tests for the SPI half of the bus observation port
// (libraries/SPI/src/SPI.h).
//
// The hook sees every transferred byte and its return value becomes what
// the sketch reads back, so a library-side device model can both watch
// the conversation and answer on MISO. The transaction hook exposes
// SPISettings as the sketch passed them, which is how a test asserts the
// bus is being driven at the clock / bit order / mode the device wants.

#include <Arduino.h>
#include <SPI.h>

namespace {

constexpr int PIN_SCK = 18;
constexpr int PIN_MISO = 19;
constexpr int PIN_MOSI = 23;
constexpr int PIN_SS = 5;

struct DeviceModel {
    uint8_t seen[16] = {0};
    uint8_t seen_count = 0;
    uint8_t transactions = 0;
    uint32_t clock = 0;
    uint8_t bit_order = 0xFF;
    uint8_t data_mode = 0xFF;
    bool active = false;
};

DeviceModel model;

uint8_t onTransfer(uint8_t out, void *user)
{
    DeviceModel *m = static_cast<DeviceModel *>(user);
    if (m->seen_count < sizeof(m->seen)) {
        m->seen[m->seen_count++] = out;
    }
    // Answer on MISO: the model echoes the complement. A write-only
    // device (a display panel) would return anything and the sketch would
    // ignore it.
    return static_cast<uint8_t>(~out);
}

void onTransaction(bool active, const SPISettings &settings, void *user)
{
    DeviceModel *m = static_cast<DeviceModel *>(user);
    m->active = active;
    if (active) {
        ++m->transactions;
        m->clock = settings._clock;
        m->bit_order = settings._bitOrder;
        m->data_mode = settings._dataMode;
    }
}

} // namespace

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start spi_hook");

    SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_SS);
    Serial.printf("pins: sck=%d miso=%d mosi=%d ss=%d begun=%d\n", SPI.sck(), SPI.miso(), SPI.mosi(),
                  SPI.ss(), SPI.begun() ? 1 : 0);

    // With nothing attached, a transfer reads back an idle bus.
    const uint8_t idle = SPI.transfer(0x9F);
    Serial.printf("nohook: read=%02X count=%u\n", idle, SPI.transferCount());

    SPI.setTransferHook(onTransfer, &model);
    SPI.setTransactionHook(onTransaction, &model);
    SPI.resetTransferCount();

    SPI.beginTransaction(SPISettings(24000000, MSBFIRST, SPI_MODE0));
    Serial.printf("settings: clock=%u order=%u mode=%u active=%d in=%d\n", model.clock, model.bit_order,
                  model.data_mode, model.active ? 1 : 0, SPI.inTransaction() ? 1 : 0);

    // Bytes reach the model, and its answer reaches the sketch.
    const uint8_t response = SPI.transfer(0x2A);
    Serial.printf("miso: sent=2A read=%02X\n", response);

    // 16-bit transfers split MSB first while the bus is MSBFIRST.
    const uint16_t wide = SPI.transfer16(0xBEEF);
    Serial.printf("wide: read=%04X\n", wide);

    // In-place buffer transfer: each byte is replaced by the answer.
    uint8_t buffer[3] = {0x01, 0x02, 0x03};
    SPI.transfer(buffer, sizeof(buffer));
    Serial.printf("buffer: %02X,%02X,%02X\n", buffer[0], buffer[1], buffer[2]);

    // Write-only path: same hook, answers discarded.
    const uint8_t pixels[2] = {0xF8, 0x00};
    SPI.writeBytes(pixels, sizeof(pixels));

    SPI.endTransaction();
    Serial.printf("end: active=%d in=%d transactions=%u\n", model.active ? 1 : 0,
                  SPI.inTransaction() ? 1 : 0, model.transactions);

    Serial.print("seen=");
    for (uint8_t i = 0; i < model.seen_count; ++i) {
        Serial.printf("%s%02X", i ? "," : "", model.seen[i]);
    }
    Serial.println();
    Serial.printf("count=%u\n", SPI.transferCount());

    // A second transaction re-reports the settings, so a test can catch a
    // sketch that talks to two devices at different modes.
    SPI.beginTransaction(SPISettings(1000000, LSBFIRST, SPI_MODE3));
    Serial.printf("second: clock=%u order=%u mode=%u\n", model.clock, model.bit_order, model.data_mode);
    SPI.endTransaction();

    SPI.clearHooks();
    Serial.printf("cleared: read=%02X\n", SPI.transfer(0x11));

    SPI.end();
    Serial.printf("ended: begun=%d\n", SPI.begun() ? 1 : 0);

    Serial.println("TEST done");
}

void loop()
{
    delay(10);
}
