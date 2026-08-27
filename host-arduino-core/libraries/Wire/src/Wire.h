#ifndef HOST_ARDUINO_WIRE_H
#define HOST_ARDUINO_WIRE_H

#include <Arduino.h>

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
// The hooks are transaction-granular, not byte-granular like SPI's. That
// is the level an I2C device model actually works at: a write transaction
// is an address plus a payload plus a stop condition, and a read
// transaction is a request for N bytes that the device answers as a
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
// role (`onReceive` / `onRequest` are accepted and never called).

#ifndef I2C_BUFFER_LENGTH
#define I2C_BUFFER_LENGTH 128
#endif

class TwoWire : public Stream {
public:
    // Return value is the `endTransmission` status the sketch sees:
    //   0 = success (ACK), 1 = data too long, 2 = address NACK,
    //   3 = data NACK, 4 = other error.
    using WriteHook = uint8_t (*)(uint8_t address, const uint8_t *data, size_t len, bool stop, void *user);

    // Fill up to `len` bytes and return how many were supplied. Returning
    // 0 means "no device at this address", which is what `requestFrom`
    // reports back.
    using ReadHook = size_t (*)(uint8_t address, uint8_t *data, size_t len, bool stop, void *user);

    explicit TwoWire(uint8_t bus_num = 0) : _bus(bus_num) {}

    // --- Arduino / ESP32 surface -------------------------------------

    // Master mode. The slave overload deliberately has no default
    // arguments, matching arduino-esp32: with defaults on both, a call
    // like `begin(21, 22, 400000)` would be ambiguous between them.
    bool begin(int sda = -1, int scl = -1, uint32_t frequency = 0);
    bool begin(uint8_t address, int sda, int scl, uint32_t frequency);
    bool end();

    bool setPins(int sda, int scl);
    bool setClock(uint32_t frequency);
    uint32_t getClock() const { return _frequency; }
    void setTimeOut(uint16_t timeout_ms) { _timeout = timeout_ms; }
    uint16_t getTimeOut() const { return _timeout; }
    void setBufferSize(size_t) {}

    void beginTransmission(uint16_t address);
    void beginTransmission(uint8_t address) { beginTransmission(static_cast<uint16_t>(address)); }
    void beginTransmission(int address) { beginTransmission(static_cast<uint16_t>(address)); }

    uint8_t endTransmission(bool sendStop);
    uint8_t endTransmission() { return endTransmission(true); }

    size_t requestFrom(uint16_t address, size_t size, bool sendStop);
    uint8_t requestFrom(uint16_t address, uint8_t size, bool sendStop);
    uint8_t requestFrom(uint16_t address, uint8_t size);
    uint8_t requestFrom(uint8_t address, uint8_t size, bool sendStop);
    uint8_t requestFrom(uint8_t address, uint8_t size);
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
    void onReceive(void (*callback)(int)) { _onReceive = callback; }
    void onRequest(void (*callback)(void)) { _onRequest = callback; }
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

    void clearHooks()
    {
        _writeHook = nullptr;
        _writeHookUser = nullptr;
        _readHook = nullptr;
        _readHookUser = nullptr;
    }

    bool begun() const { return _begun; }
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

    void (*_onReceive)(int) = nullptr;
    void (*_onRequest)(void) = nullptr;
};

extern TwoWire Wire;
extern TwoWire Wire1;

#endif
