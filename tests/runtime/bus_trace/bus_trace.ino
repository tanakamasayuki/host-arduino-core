// One ordered trace across every half of the bus observation port.
//
// The other hook tests each check one bus in isolation. This one checks
// the thing those cannot: that a driver's *whole* startup — I2C brought
// up, a reset pulse bit-banged, SPI brought up, the backlight configured
// and lit, commands pushed, the touch controller probed — lands in a
// single sequence a test can compare against a golden list, line for
// line. Getting a step out of order is a real class of display-driver bug
// and it is invisible to an end-state assertion.
//
// The pattern: register every hook, append one line per event to a
// buffer, print the buffer once at the end. Recording rather than
// printing from inside the hooks is what keeps the trace independent of
// whatever else the sketch is writing to Serial.

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#include <stdarg.h>

namespace {

constexpr int PIN_SCK = 18;
constexpr int PIN_MISO = 19;
constexpr int PIN_MOSI = 23;
constexpr int PIN_CS = 5;
constexpr int PIN_RST = 33;
constexpr int PIN_BACKLIGHT = 38;

constexpr int PIN_SDA = 21;
constexpr int PIN_SCL = 22;
constexpr uint8_t TOUCH_ADDR = 0x38;

constexpr uint8_t kTraceMax = 48;

struct Trace {
    char line[kTraceMax][48] = {{0}};
    uint8_t count = 0;
    bool overflowed = false;

    void add(const char *format, ...) __attribute__((format(printf, 2, 3)))
    {
        if (count >= kTraceMax) {
            overflowed = true;
            return;
        }
        va_list args;
        va_start(args, format);
        vsnprintf(line[count], sizeof(line[0]), format, args);
        va_end(args);
        ++count;
    }
};

Trace trace;

// --- GPIO half -------------------------------------------------------

void onPinMode(uint8_t pin, uint8_t mode, void *user)
{
    static_cast<Trace *>(user)->add("gpio.mode pin=%u mode=%u", pin, mode);
}

void onPinWrite(uint8_t pin, uint8_t value, void *user)
{
    static_cast<Trace *>(user)->add("gpio.write pin=%u value=%u", pin, value);
}

// --- Analog / PWM half -----------------------------------------------

void onAnalogWrite(HostArduino::AnalogWriteEvent event, const HostArduino::AnalogOut &out, void *user)
{
    Trace *t = static_cast<Trace *>(user);
    switch (event) {
    case HostArduino::kAnalogAttach:
        t->add("pwm.attach pin=%u ch=%u f=%u r=%u", out.pin, out.channel, out.frequency, out.resolution);
        break;
    case HostArduino::kAnalogWrite:
        t->add("pwm.write pin=%u duty=%u", out.pin, out.duty);
        break;
    case HostArduino::kAnalogDetach:
        t->add("pwm.detach pin=%u", out.pin);
        break;
    default:
        t->add("pwm.other pin=%u event=%d", out.pin, static_cast<int>(event));
        break;
    }
}

// --- SPI half --------------------------------------------------------

void onSpiLifecycle(SPIClass::LifecycleEvent event, const SPIClass &spi, void *user)
{
    Trace *t = static_cast<Trace *>(user);
    switch (event) {
    case SPIClass::kBegin:
        t->add("spi.begin sck=%d mosi=%d cs=%d", spi.sck(), spi.mosi(), spi.ss());
        break;
    case SPIClass::kEnd:
        t->add("spi.end");
        break;
    case SPIClass::kConfig:
        t->add("spi.config clock=%u mode=%u", spi.settings().clock(), spi.settings().dataMode());
        break;
    }
}

void onSpiTransaction(bool active, const SPISettings &settings, void *user)
{
    static_cast<Trace *>(user)->add("spi.txn active=%d clock=%u mode=%u", active ? 1 : 0, settings.clock(),
                                    settings.dataMode());
}

uint8_t onSpiTransfer(uint8_t out, void *user)
{
    // A panel is write-only. Reading CS back through the GPIO half is how
    // a model checks it is actually being addressed — the same trick a
    // real one uses for the DC line.
    static_cast<Trace *>(user)->add("spi.byte %02X cs=%d", out, digitalRead(PIN_CS));
    return 0xFF;
}

// --- I2C half --------------------------------------------------------

void onWireLifecycle(TwoWire::LifecycleEvent event, const TwoWire &wire, void *user)
{
    Trace *t = static_cast<Trace *>(user);
    switch (event) {
    case TwoWire::kBegin:
        t->add("i2c.begin sda=%d scl=%d clock=%u", wire.sda(), wire.scl(), wire.getClock());
        break;
    case TwoWire::kEnd:
        t->add("i2c.end");
        break;
    case TwoWire::kSetClock:
        t->add("i2c.clock %u", wire.getClock());
        break;
    case TwoWire::kSetPins:
        t->add("i2c.pins sda=%d scl=%d", wire.sda(), wire.scl());
        break;
    case TwoWire::kSetTimeout:
        t->add("i2c.timeout %u", wire.getTimeOut());
        break;
    }
}

uint8_t onWireWrite(uint8_t address, const uint8_t *data, size_t len, bool stop, void *user)
{
    Trace *t = static_cast<Trace *>(user);
    t->add("i2c.write addr=%02X len=%u stop=%d", address, (unsigned)len, stop ? 1 : 0);
    if (address != TOUCH_ADDR) {
        return 2; // address NACK
    }
    (void)data;
    return 0;
}

size_t onWireRead(uint8_t address, uint8_t *data, size_t len, bool stop, void *user)
{
    Trace *t = static_cast<Trace *>(user);
    t->add("i2c.read addr=%02X len=%u stop=%d", address, (unsigned)len, stop ? 1 : 0);
    if (address != TOUCH_ADDR || len == 0) {
        return 0;
    }
    data[0] = 0xA8; // the touch controller's chip id
    return 1;
}

// What a display driver's begin() would do, in the order it would do it.
void panelBegin()
{
    // 1. the touch controller shares the board's I2C bus
    Wire.begin(PIN_SDA, PIN_SCL, 100000);
    Wire.setClock(400000);

    // 2. hardware reset, bit-banged on a plain GPIO
    pinMode(PIN_RST, OUTPUT);
    digitalWrite(PIN_RST, LOW);
    digitalWrite(PIN_RST, HIGH);

    // 3. the panel's SPI bus
    pinMode(PIN_CS, OUTPUT);
    digitalWrite(PIN_CS, HIGH);
    SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

    // 4. backlight configured but dark, so the init sequence is not seen
    ledcAttach(PIN_BACKLIGHT, 5000, 8);
    ledcWrite(PIN_BACKLIGHT, 0);

    // 5. the init commands
    SPI.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(0x01); // software reset
    SPI.transfer(0x11); // sleep out
    digitalWrite(PIN_CS, HIGH);
    SPI.endTransaction();

    // 6. and only now bring the backlight up
    ledcWrite(PIN_BACKLIGHT, 200);

    // 7. probe the touch controller
    Wire.beginTransmission(TOUCH_ADDR);
    Wire.write(0xA3);
    Wire.endTransmission();
    Wire.requestFrom(TOUCH_ADDR, (uint8_t)1);
}

} // namespace

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start bus_trace");

    HostArduino::setPinModeHook(onPinMode, &trace);
    HostArduino::setPinWriteHook(onPinWrite, &trace);
    HostArduino::setAnalogWriteHook(onAnalogWrite, &trace);
    SPI.setLifecycleHook(onSpiLifecycle, &trace);
    SPI.setTransactionHook(onSpiTransaction, &trace);
    SPI.setTransferHook(onSpiTransfer, &trace);
    Wire.setLifecycleHook(onWireLifecycle, &trace);
    Wire.setWriteHook(onWireWrite, &trace);
    Wire.setReadHook(onWireRead, &trace);

    panelBegin();

    const int chip_id = Wire.read();
    Serial.printf("trace=%u overflow=%d chip=%02X\n", trace.count, trace.overflowed ? 1 : 0, chip_id);
    for (uint8_t i = 0; i < trace.count; ++i) {
        Serial.printf("| %s\n", trace.line[i]);
    }

    // Releasing every slot leaves an unobserved bus behind, which is how
    // one test hands the buses to the next model.
    HostArduino::clearPinHooks();
    HostArduino::clearAnalogHooks();
    SPI.clearHooks();
    Wire.clearHooks();

    const uint8_t before = trace.count;
    digitalWrite(PIN_RST, LOW);
    ledcWrite(PIN_BACKLIGHT, 10);
    SPI.transfer(0x00);
    Wire.beginTransmission(TOUCH_ADDR);
    Wire.endTransmission();
    Wire.end();
    Serial.printf("released: delta=%u\n", trace.count - before);

    Serial.println("TEST done");
}

void loop()
{
    delay(10);
}
