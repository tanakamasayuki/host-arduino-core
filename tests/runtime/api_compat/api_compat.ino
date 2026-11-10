// API surface that drivers written against ArduinoCore-API (or AVR) expect
// and that arduino-esp32-shaped host code used to lack. Two halves:
//
//   1. Things that only have to compile. Each line below was a build
//      failure (or a silent misbinding) before; the sketch building at all
//      is the assertion, and the line printed next to it is just a marker.
//   2. Things with behavior: what Wire.begin(address) records, that a
//      requestFrom length is not truncated to a byte, the timeout flag,
//      the interrupt-enable flag, setPins, and Servo's PWM.
//
// The list in part 1 is the -D probe table from EmbedBench's
// docs/HOST_CORE.ja.md §3.5, folded into one sketch.

#include <Arduino.h>
#include <HardwareI2C.h>
#include <HardwareSPI.h>
#include <SPI.h>
#include <Servo.h>
#include <Wire.h>

// --- compile-only checks ---------------------------------------------

#ifndef SPI_HAS_TRANSACTION
#error SPI_HAS_TRANSACTION
#endif
#ifndef WIRE_HAS_END
#error WIRE_HAS_END
#endif
#ifndef WIRE_HAS_TIMEOUT
#error WIRE_HAS_TIMEOUT
#endif
#ifndef MSBFIRST
#error MSBFIRST must stay defined for #ifdef checks
#endif
#ifndef SPI_MODE0
#error SPI_MODE0 must stay defined for #ifdef checks
#endif
#if OUTPUT_OPENDRAIN != OUTPUT_OPEN_DRAIN
#error OUTPUT_OPENDRAIN
#endif

// The shape that stopped Adafruit BusIO: a BitOrder parameter whose
// default argument is MSBFIRST.
static int takesOrder(BitOrder order = MSBFIRST) { return static_cast<int>(order); }
static int takesMode(SPIMode mode = SPI_MODE3) { return static_cast<int>(mode); }

// A driver that takes "any bus" by the ArduinoCore-API base classes.
static size_t readVia(arduino::HardwareI2C &bus, uint8_t addr, size_t len)
{
    return bus.requestFrom(addr, len);
}
static uint8_t transferVia(arduino::HardwareSPI &bus, uint8_t out) { return bus.transfer(out); }

static void IRAM_ATTR isrAttr() {}

// --- device models -------------------------------------------------------

namespace {

// Answers every read with as many bytes as asked for.
size_t fillAll(uint8_t, uint8_t *data, size_t len, bool, void *)
{
    for (size_t i = 0; i < len; ++i) {
        data[i] = static_cast<uint8_t>(i);
    }
    return len;
}

// Answers a write with the status it was given.
uint8_t writeStatus(uint8_t, const uint8_t *, size_t, bool, void *user)
{
    return *static_cast<uint8_t *>(user);
}

uint8_t echoSpi(uint8_t out, void *) { return static_cast<uint8_t>(out ^ 0xFF); }

int g_param_seen = 0;
void paramIsr(void *p) { g_param_seen = *static_cast<int *>(p); }

char g_servo[160];
void onAnalog(HostArduino::AnalogWriteEvent event, const HostArduino::AnalogOut &out, void *)
{
    const char *name = event == HostArduino::kAnalogAttach   ? "attach"
                       : event == HostArduino::kAnalogWrite  ? "write"
                       : event == HostArduino::kAnalogDetach ? "detach"
                                                             : "other";
    char line[64];
    snprintf(line, sizeof(line), "%s%s:%u/%u/%u", g_servo[0] ? "," : "", name, (unsigned)out.frequency,
             (unsigned)out.resolution, (unsigned)out.duty);
    strncat(g_servo, line, sizeof(g_servo) - strlen(g_servo) - 1);
}

} // namespace

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start api_compat");

    // §3.5 probes. Every one of these failed to build on 1.7.1, or (the
    // slave one) built and did the wrong thing.
    noInterrupts();
    const bool masked = !HostArduino::interruptsEnabled();
    interrupts();
    const bool unmasked = HostArduino::interruptsEnabled();
    Serial.printf("interrupts: masked=%d unmasked=%d\n", masked ? 1 : 0, unmasked ? 1 : 0);

    Wire.setReadHook(fillAll);
    const size_t rf_size = Wire.requestFrom(uint8_t(0x44), size_t(6), true);
    const size_t rf_int = Wire.requestFrom(0x44, size_t(6), true);
    const size_t rf_u8 = Wire.requestFrom(uint8_t(0x44), uint8_t(6), uint8_t(1));
    const size_t rf_two = Wire.requestFrom(uint8_t(0x44), size_t(6));
    Serial.printf("requestFrom: size=%u int=%u u8=%u two=%u\n", (unsigned)rf_size, (unsigned)rf_int,
                  (unsigned)rf_u8, (unsigned)rf_two);

    // A length above 255 used to be cut to its low byte (300 -> 44). It is
    // clamped to the buffer instead.
    const size_t rf_big = Wire.requestFrom(0x44, 300);
    Serial.printf("requestFrom: big=%u buffer=%u\n", (unsigned)rf_big, (unsigned)BUFFER_LENGTH);

    Serial.printf("wire: interfaces=%d buf64=%u buf256=%u\n", WIRE_INTERFACES_COUNT,
                  (unsigned)Wire.setBufferSize(64), (unsigned)Wire.setBufferSize(256));

    arduino::HardwareI2C *h = &Wire;
    Serial.printf("hwi2c: read=%u\n", (unsigned)readVia(*h, 0x44, 3));

    // One-argument begin is the slave role, as on arduino-esp32.
    const bool slave_ok = Wire.begin(uint8_t(0x08));
    Serial.printf("slave: ok=%d slave=%d addr=%u\n", slave_ok ? 1 : 0, Wire.slave() ? 1 : 0,
                  Wire.slaveAddress());
    Wire.begin();
    Serial.printf("master: slave=%d sda=%d scl=%d\n", Wire.slave() ? 1 : 0, Wire.sda(), Wire.scl());
    Wire.begin(21, 22, 400000);
    Serial.printf("master_pins: slave=%d sda=%d scl=%d\n", Wire.slave() ? 1 : 0, Wire.sda(), Wire.scl());

    // AVR timeout surface. The flag is raised by a model answering 5.
    Wire.setWireTimeout(25000, true);
    Serial.printf("timeout: ms=%u reset=%d flag=%d\n", Wire.getTimeOut(), Wire.wireTimeoutReset() ? 1 : 0,
                  Wire.getWireTimeoutFlag() ? 1 : 0);
    static uint8_t status = 5;
    Wire.setWriteHook(writeStatus, &status);
    Wire.beginTransmission(0x44);
    Wire.write(0x00);
    const uint8_t timed_out = Wire.endTransmission();
    const bool flag_set = Wire.getWireTimeoutFlag();
    Wire.clearWireTimeoutFlag();
    Serial.printf("timeout: status=%u flag=%d cleared=%d\n", timed_out, flag_set ? 1 : 0,
                  Wire.getWireTimeoutFlag() ? 1 : 0);
    Wire.setWireTimeout(1500);
    Serial.printf("timeout: rounded_ms=%u\n", Wire.getTimeOut());
    Wire.clearHooks();

    uint8_t buf[BUFFER_LENGTH];
    (void)buf;

    // SPI.
    SPI.setPins(1, 2, 3);
    SPI.begin();
    Serial.printf("spi: sck=%d miso=%d mosi=%d ss=%d interfaces=%d\n", SPI.sck(), SPI.miso(), SPI.mosi(),
                  SPI.ss(), SPI_INTERFACES_COUNT);
    SPI.setClockDivider(SPI_CLOCK_DIV4);
    Serial.printf("spi: div4=%08lx\n", (unsigned long)SPI.getClockDivider());
    const SPISettings s(1000000, MSBFIRST, SPI_MODE0);
    SPI.beginTransaction(s);
    SPI.setTransferHook(echoSpi);
    arduino::HardwareSPI &hs = SPI;
    Serial.printf("hwspi: out=%02x\n", transferVia(hs, 0x5A));
    SPI.endTransaction();
    SPI.clearHooks();
    Serial.printf("settings: clock=%lu order=%d mode=%d eq=%d\n", (unsigned long)s.getClockFreq(),
                  (int)s.getBitOrder(), (int)s.getDataMode(),
                  s == SPISettings(1000000, MSBFIRST, SPI_MODE0) ? 1 : 0);

    // Core constants.
    Serial.printf("cpu: f=%lu cpm=%ld us2c=%ld c2us=%ld noint=%d\n", (unsigned long)F_CPU,
                  clockCyclesPerMicrosecond(), (long)microsecondsToClockCycles(2),
                  (long)clockCyclesToMicroseconds(480), NOT_AN_INTERRUPT);
    Serial.printf("enums: order=%d lsb=%d mode=%d\n", takesOrder(), takesOrder(LSBFIRST), takesMode());
    pinMode(5, OUTPUT_OPENDRAIN);
    const PinStatus ps = digitalRead(5);
    (void)ps;
    isrAttr();

    // attachInterruptParam: (pin, handler, mode, param) — the mode before
    // the argument, unlike attachInterruptArg.
    static int param = 42;
    attachInterruptParam(4, paramIsr, RISING, &param);
    HostArduino::triggerInterrupt(4);
    detachInterrupt(4);
    Serial.printf("param: seen=%d\n", g_param_seen);

    // Servo over LEDC.
    HostArduino::setAnalogWriteHook(onAnalog);
    Servo servo;
    const uint8_t ch = servo.attach(13);
    servo.write(90);
    const int angle = servo.read();
    const int us = servo.readMicroseconds();
    servo.writeMicroseconds(3000);
    const int clamped = servo.readMicroseconds();
    servo.detach();
    HostArduino::setAnalogWriteHook(nullptr);
    Serial.printf("servo: ch=%u angle=%d us=%d clamped=%d attached=%d back=%lu\n", ch, angle, us, clamped,
                  servo.attached() ? 1 : 0, (unsigned long)Servo::pulseFromDuty(Servo::dutyFromPulse(1472)));
    Serial.printf("servo_trace: %s\n", g_servo);

    Serial.println("TEST done");
}

void loop()
{
    delay(10);
}
