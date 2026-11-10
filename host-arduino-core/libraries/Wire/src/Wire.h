#ifndef HOST_ARDUINO_WIRE_H
#define HOST_ARDUINO_WIRE_H

#include <Arduino.h>
#include <HardwareI2C.h>

#include <stddef.h>
#include <stdint.h>

// Bus observation port — I2C half. The GPIO half is in
// cores/host/HostBus.h, the SPI half on `SPIClass`.
//
// `Wire` initializes successfully and, by default, finds nothing on the
// bus: `endTransmission()` returns 2 (address NACK) and `requestFrom()`
// returns 0. That is the honest answer for a host with no hardware, and
// it is what a scan loop expects to see for an empty address.
//
// A third hook covers the bus's own lifecycle — `begin()`, `end()`, and
// the configuration setters. The pins and the clock were always readable
// through `sda()` / `scl()` / `getClock()`, but reading them afterwards
// cannot tell you *when* the bus was brought up relative to everything
// else a driver did. That ordering is the thing a test compares against a
// golden trace: an ST7789 reset before or after `Wire.begin()` is the
// difference between a driver that works and one that does not.
//
// The transaction hooks are transaction-granular, not byte-granular like
// SPI's. That is the level an I2C device model actually works at: a write
// transaction is an address plus a payload plus a stop condition, and a
// read transaction is a request for N bytes that the device answers as a
// block. Register a device model like this:
//
//   static uint8_t onWrite(uint8_t addr, const uint8_t *data, size_t len,
//                          bool stop, void *user)
//   {
//       if (addr != 0x68) return 2;                  // nobody home
//       static_cast<MySensor *>(user)->command(data, len);
//       return 0;                                    // ACK
//   }
//   static size_t onRead(uint8_t addr, uint8_t *data, size_t len,
//                        bool stop, void *user)
//   {
//       if (addr != 0x68) return 0;
//       return static_cast<MySensor *>(user)->fill(data, len);
//   }
//   Wire.setWriteHook(onWrite, &sensor);
//   Wire.setReadHook(onRead, &sensor);
//
// A library that bit-bangs I2C over `digitalWrite` instead of using this
// class is served by the GPIO half of the port, not by these hooks.
//
// Not modelled: clock stretching, arbitration, bus timing, and the slave
// role. `begin(address)` is accepted and recorded (`slave()`), and
// `onReceive` / `onRequest` are kept, but nothing ever addresses the
// sketch as a slave, so the callbacks are never called.

#ifndef I2C_BUFFER_LENGTH
#define I2C_BUFFER_LENGTH 128
#endif

// Feature macros drivers test with `#ifdef`. WIRE_HAS_END and
// WIRE_HAS_BUFFER_SIZE are arduino-esp32's; WIRE_HAS_TIMEOUT is AVR's and
// announces setWireTimeout / getWireTimeoutFlag / clearWireTimeoutFlag.
#define WIRE_HAS_END 1
#define WIRE_HAS_BUFFER_SIZE 1
#define WIRE_HAS_TIMEOUT 1

// AVR / ArduinoCore-API names. BUFFER_LENGTH is this core's buffer size,
// not AVR's 32: it is how much one transaction can carry here.
#ifndef BUFFER_LENGTH
#define BUFFER_LENGTH I2C_BUFFER_LENGTH
#endif
#ifndef WIRE_INTERFACES_COUNT
#define WIRE_INTERFACES_COUNT 2
#endif

class TwoWire : public HardwareI2C {
public:
    // Return value is the `endTransmission` status the sketch sees:
    //   0 = success (ACK), 1 = data too long, 2 = address NACK,
    //   3 = data NACK, 4 = other error.
    using WriteHook = uint8_t (*)(uint8_t address, const uint8_t *data, size_t len, bool stop, void *user);

    // Fill up to `len` bytes and return how many were supplied. Returning
    // 0 means "no device at this address", which is what `requestFrom`
    // reports back.
    using ReadHook = size_t (*)(uint8_t address, uint8_t *data, size_t len, bool stop, void *user);

    // Bus setup, in the order the sketch performed it. `kBegin` covers
    // both the master and the slave overload; `slave()` on the object the
    // hook receives tells them apart.
    enum LifecycleEvent : uint8_t {
        kBegin = 0,
        kEnd,
        kSetPins,
        kSetClock,
        kSetTimeout,
    };

    // Called after the change has been applied, so `wire` already reports
    // the new state. The whole object is passed rather than the changed
    // field: `busNum()` is what tells `Wire` from `Wire1` when one model
    // watches both, and a lifecycle trace usually wants to print the pins
    // and the clock together anyway.
    using LifecycleHook = void (*)(LifecycleEvent event, const TwoWire &wire, void *user);

    explicit TwoWire(uint8_t bus_num = 0) : _bus(bus_num) {}

    // --- Arduino / ESP32 surface -------------------------------------

    // The overload set is arduino-esp32 3.x's, and so is what each call
    // means:
    //   begin()                    master, default pins
    //   begin(sda, scl[, freq])    master on those pins
    //   begin(address)             SLAVE at `address` (any one-argument
    //                              call, `Wire.begin(8)` included)
    //   begin(address, sda, scl, freq)  slave on those pins
    // The slave role is recorded, reported once as a [HostCore] hint, and
    // otherwise not modelled.
    bool begin() override { return begin(-1, -1, 0); }
    bool begin(uint8_t address) override { return begin(address, -1, -1, 0); }
    bool begin(int sda, int scl, uint32_t frequency = 0);
    bool begin(uint8_t address, int sda, int scl, uint32_t frequency);
    bool end() override;

    bool setPins(int sda, int scl);
    bool setClock(uint32_t frequency) override;
    uint32_t getClock() const { return _frequency; }
    void setTimeOut(uint16_t timeout_ms);
    uint16_t getTimeOut() const { return _timeout; }
    // The buffers are fixed at I2C_BUFFER_LENGTH. Returns the size in
    // force when the request fits in it and 0 when it does not, which is
    // arduino-esp32's "could not allocate" answer.
    size_t setBufferSize(size_t size) { return size <= I2C_BUFFER_LENGTH ? I2C_BUFFER_LENGTH : 0; }

    // AVR timeout surface, mapped onto setTimeOut: microseconds rounded up
    // to whole milliseconds, 0 meaning "no timeout" as on AVR. `reset` is
    // recorded and has nothing to reset.
    //
    // A host bus cannot hang, so the core never sets the flag by itself.
    // A device model sets it by answering a write with status 5 — the
    // status AVR's endTransmission returns on a timeout — which is how a
    // test exercises a driver's timeout path.
    void setWireTimeout(uint32_t timeout_us = 25000, bool reset_with_timeout = false);
    bool getWireTimeoutFlag() const { return _timeoutFlag; }
    void clearWireTimeoutFlag() { _timeoutFlag = false; }
    bool wireTimeoutReset() const { return _timeoutReset; }

    void beginTransmission(uint16_t address);
    void beginTransmission(uint8_t address) override { beginTransmission(static_cast<uint16_t>(address)); }
    void beginTransmission(int address) { beginTransmission(static_cast<uint16_t>(address)); }

    uint8_t endTransmission(bool sendStop) override;
    uint8_t endTransmission() override { return endTransmission(true); }

    // `(uint8_t, size_t, bool)` / `(uint8_t, size_t)` are the base class's
    // (ArduinoCore-API's and arduino-esp32's) shape; `(int, size_t, bool)`
    // exists so a literal address next to a size_t length is not ambiguous
    // between the uint8_t and uint16_t address overloads. Lengths are never
    // narrowed to a byte on the way in: every overload clamps to the
    // buffer, so the uint8_t-returning ones cannot overflow either.
    size_t requestFrom(uint8_t address, size_t size, bool sendStop) override
    {
        return requestFrom(static_cast<uint16_t>(address), size, sendStop);
    }
    size_t requestFrom(uint8_t address, size_t size) override
    {
        return requestFrom(static_cast<uint16_t>(address), size, true);
    }
    size_t requestFrom(int address, size_t size, bool sendStop)
    {
        return requestFrom(static_cast<uint16_t>(address), size, sendStop);
    }
    size_t requestFrom(uint16_t address, size_t size, bool sendStop);
    uint8_t requestFrom(uint16_t address, uint8_t size, bool sendStop);
    uint8_t requestFrom(uint16_t address, uint8_t size);
    uint8_t requestFrom(uint8_t address, uint8_t size, bool sendStop);
    uint8_t requestFrom(uint8_t address, uint8_t size);
    // AVR's three-byte form. Without it a call written that way (Adafruit
    // BusIO's) only resolved through a GCC extension, with a warning.
    uint8_t requestFrom(uint8_t address, uint8_t size, uint8_t sendStop)
    {
        return requestFrom(address, size, sendStop != 0);
    }
    uint8_t requestFrom(int address, int size, int sendStop);
    uint8_t requestFrom(int address, int size);

    size_t write(uint8_t data) override;
    size_t write(const uint8_t *data, size_t size) override;

    // The integral overloads are the AVR Wire spelling. They exist so
    // `Wire.write(0x00)` picks a byte write instead of being ambiguous
    // against `Print::write(const char *)` — a literal 0 is also a null
    // pointer constant, and both conversions rank equally.
    size_t write(unsigned long n) { return write(static_cast<uint8_t>(n)); }
    size_t write(long n) { return write(static_cast<uint8_t>(n)); }
    size_t write(unsigned int n) { return write(static_cast<uint8_t>(n)); }
    size_t write(int n) { return write(static_cast<uint8_t>(n)); }
    using Print::write;

    int available() override;
    int read() override;
    int peek() override;
    void flush() override;

    // Slave role. Recorded so a sketch compiles; never invoked.
    void onReceive(const std::function<void(int)> &callback) override { _onReceive = callback; }
    void onRequest(const std::function<void()> &callback) override { _onRequest = callback; }
    size_t slaveWrite(const uint8_t *, size_t) { return 0; }

    uint8_t busNum() const { return _bus; }

    // --- Observation port -------------------------------------------

    void setWriteHook(WriteHook hook, void *user = nullptr)
    {
        _writeHook = hook;
        _writeHookUser = user;
    }

    void setReadHook(ReadHook hook, void *user = nullptr)
    {
        _readHook = hook;
        _readHookUser = user;
    }

    void setLifecycleHook(LifecycleHook hook, void *user = nullptr)
    {
        _lifecycleHook = hook;
        _lifecycleHookUser = user;
    }

    void clearHooks()
    {
        _writeHook = nullptr;
        _writeHookUser = nullptr;
        _readHook = nullptr;
        _readHookUser = nullptr;
        _lifecycleHook = nullptr;
        _lifecycleHookUser = nullptr;
    }

    bool begun() const { return _begun; }
    // True after a slave `begin(address, ...)`, false after a master one.
    // `slaveAddress()` is the address it was given.
    bool slave() const { return _slave; }
    uint8_t slaveAddress() const { return _slaveAddress; }
    int sda() const { return _sda; }
    int scl() const { return _scl; }

    // Address of the transaction in progress, or the last one completed.
    uint16_t lastAddress() const { return _address; }
    // Transactions attempted since the last reset, hook or no hook.
    uint32_t writeCount() const { return _writeCount; }
    uint32_t readCount() const { return _readCount; }
    void resetCounts()
    {
        _writeCount = 0;
        _readCount = 0;
    }

private:
    uint8_t _bus;
    bool _begun = false;
    bool _slave = false;
    uint8_t _slaveAddress = 0;
    bool _timeoutFlag = false;
    bool _timeoutReset = false;
    int _sda = -1;
    int _scl = -1;
    uint32_t _frequency = 100000;
    uint16_t _timeout = 50;
    uint16_t _address = 0;
    bool _transmitting = false;
    uint32_t _writeCount = 0;
    uint32_t _readCount = 0;

    uint8_t _txBuffer[I2C_BUFFER_LENGTH] = {0};
    size_t _txLength = 0;
    bool _txOverflow = false;

    uint8_t _rxBuffer[I2C_BUFFER_LENGTH] = {0};
    size_t _rxLength = 0;
    size_t _rxIndex = 0;

    WriteHook _writeHook = nullptr;
    void *_writeHookUser = nullptr;
    ReadHook _readHook = nullptr;
    void *_readHookUser = nullptr;
    LifecycleHook _lifecycleHook = nullptr;
    void *_lifecycleHookUser = nullptr;

    void reportLifecycle(LifecycleEvent event)
    {
        if (_lifecycleHook) {
            _lifecycleHook(event, *this, _lifecycleHookUser);
        }
    }

    std::function<void(int)> _onReceive;
    std::function<void()> _onRequest;
};

extern TwoWire Wire;
extern TwoWire Wire1;

#endif
