// Tests for the I2C half of the bus observation port
// (libraries/Wire/src/Wire.h).
//
// Default shape: initialization succeeds and no device answers, so
// `endTransmission()` reports an address NACK and `requestFrom()` returns
// nothing. Registering the transaction hooks puts a library-side device
// model on the bus — here a stand-in sensor at 0x68 that remembers the
// register it was pointed at and answers reads from it.

#include <Arduino.h>
#include <Wire.h>

namespace {

constexpr uint8_t SENSOR_ADDR = 0x68;

struct SensorModel {
    uint8_t registers[4] = {0xA0, 0xA1, 0xA2, 0xA3};
    uint8_t pointer = 0;
    uint8_t last_len = 0;
    bool last_stop = false;
};

SensorModel model;

uint8_t onWrite(uint8_t address, const uint8_t *data, size_t len, bool stop, void *user)
{
    SensorModel *m = static_cast<SensorModel *>(user);
    if (address != SENSOR_ADDR) {
        return 2; // nobody at this address
    }
    m->last_len = static_cast<uint8_t>(len);
    m->last_stop = stop;
    if (len >= 1) {
        m->pointer = data[0];
    }
    if (len >= 2) {
        // A register write: value follows the pointer.
        if (m->pointer < sizeof(m->registers)) {
            m->registers[m->pointer] = data[1];
        }
    }
    return 0; // ACK
}

size_t onRead(uint8_t address, uint8_t *data, size_t len, bool stop, void *user)
{
    SensorModel *m = static_cast<SensorModel *>(user);
    (void)stop;
    if (address != SENSOR_ADDR) {
        return 0;
    }
    size_t supplied = 0;
    for (size_t i = 0; i < len; ++i) {
        const uint8_t reg = static_cast<uint8_t>(m->pointer + i);
        if (reg >= sizeof(m->registers)) {
            break;
        }
        data[i] = m->registers[reg];
        ++supplied;
    }
    return supplied;
}

} // namespace

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start wire_hook");

    const bool begun = Wire.begin(21, 22, 400000);
    Serial.printf("begin=%d sda=%d scl=%d clock=%u\n", begun ? 1 : 0, Wire.sda(), Wire.scl(),
                  Wire.getClock());

    // No hook: initialization succeeded, nothing is on the bus.
    Wire.beginTransmission(SENSOR_ADDR);
    Wire.write(0x00);
    const uint8_t empty_status = Wire.endTransmission();
    const size_t empty_read = Wire.requestFrom((uint16_t)SENSOR_ADDR, (size_t)2, true);
    Serial.printf("nodevice: status=%u read=%u available=%d\n", empty_status, (unsigned)empty_read,
                  Wire.available());

    Wire.setWriteHook(onWrite, &model);
    Wire.setReadHook(onRead, &model);
    Wire.resetCounts();

    // Point the sensor at register 1 and read two bytes from it.
    Wire.beginTransmission(SENSOR_ADDR);
    Wire.write(0x01);
    const uint8_t status = Wire.endTransmission();
    Serial.printf("write: status=%u len=%u stop=%d pointer=%u\n", status, model.last_len,
                  model.last_stop ? 1 : 0, model.pointer);

    const uint8_t got = Wire.requestFrom(SENSOR_ADDR, (uint8_t)2);
    Serial.printf("read: got=%u available=%d\n", got, Wire.available());
    Serial.print("bytes=");
    while (Wire.available()) {
        Serial.printf("%02X", Wire.read());
        if (Wire.available()) {
            Serial.print(",");
        }
    }
    Serial.println();
    const int drained_available = Wire.available();
    Serial.printf("drained: available=%d read=%d\n", drained_available, Wire.read());

    // A register write reaches the model as one transaction.
    Wire.beginTransmission(SENSOR_ADDR);
    Wire.write(0x02);
    Wire.write(0x5A);
    const uint8_t poke_status = Wire.endTransmission();
    Serial.printf("poke: status=%u len=%u\n", poke_status, model.last_len);
    Wire.requestFrom(SENSOR_ADDR, (uint8_t)1);
    Serial.printf("poked: value=%02X\n", Wire.read());

    // A wrong address NACKs, which is what a scan loop is looking for.
    Wire.beginTransmission((uint8_t)0x42);
    const uint8_t nack = Wire.endTransmission();
    uint8_t found = 0;
    for (uint8_t addr = 1; addr < 0x78; ++addr) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            ++found;
        }
    }
    Serial.printf("scan: nack=%u found=%u\n", nack, found);

    // Overflowing the transmit buffer is reported without reaching the
    // model, the same way a real driver reports it.
    Wire.beginTransmission(SENSOR_ADDR);
    size_t accepted = 0;
    for (int i = 0; i < I2C_BUFFER_LENGTH + 8; ++i) {
        accepted += Wire.write((uint8_t)i);
    }
    const uint8_t overflow_status = Wire.endTransmission();
    Serial.printf("overflow: accepted=%u status=%u\n", (unsigned)accepted, overflow_status);

    Serial.printf("counts: writes=%u reads=%u\n", Wire.writeCount(), Wire.readCount());

    // Unregistering puts the empty bus back.
    Wire.clearHooks();
    Wire.beginTransmission(SENSOR_ADDR);
    Wire.write(0x00);
    const uint8_t cleared_status = Wire.endTransmission();
    const size_t cleared_read = Wire.requestFrom((uint16_t)SENSOR_ADDR, (size_t)1, true);
    Serial.printf("cleared: status=%u read=%u\n", cleared_status, (unsigned)cleared_read);

    // Wire1 exists for ESP32 sketches and is a separate bus.
    const bool wire1_begun = Wire1.begin();
    Serial.printf("wire1: begin=%d bus=%u\n", wire1_begun ? 1 : 0, Wire1.busNum());

    Serial.println("TEST done");
}

void loop()
{
    delay(10);
}
