#ifndef HOST_ARDUINO_SPI_H
#define HOST_ARDUINO_SPI_H

#include <Arduino.h>

#include <stddef.h>
#include <stdint.h>

// Bus observation port — SPI half. The GPIO half is in
// cores/host/HostBus.h; read that first, it explains why the core
// observes buses instead of modelling devices.
//
// `SPI` links and runs here, but nothing is wired to the other end of the
// bus unless a library puts something there. Every transferred byte is
// handed to a transfer hook, and the hook's return value becomes what the
// sketch reads back — so a device model on the library side sees the whole
// conversation and can answer (MISO) when it needs to:
//
//   static uint8_t onByte(uint8_t out, void *user)
//   {
//       static_cast<MyPanelModel *>(user)->feedByte(out);   // DC via digitalRead
//       return 0xFF;                                        // write-only device
//   }
//   SPI.setTransferHook(onByte, &model);
//
// The transaction hook sees `SPISettings` as the sketch passed it, which
// is how a test asserts "this panel is being driven at 24 MHz, MSB first,
// mode 0" instead of only checking the bytes.
//
// A third hook covers the bus's own lifecycle — `begin()`, `end()`, and
// the configuration setters that a driver uses instead of transactions.
// The pins were always readable through `sck()` / `miso()` / `mosi()` /
// `ss()`, but reading them afterwards cannot tell you *when* the bus came
// up relative to the reset pulse and the backlight next to it. That
// ordering is what a test compares against a golden trace.
//
// Deliberate limits:
//   - The hook is byte-granular. Bit order is reported through
//     `SPISettings`, not applied to the byte handed to the hook — the core
//     has no wire to serialize onto, so pretending otherwise would only
//     hide which setting the sketch actually chose.
//   - With no transfer hook registered, `transfer` returns 0xFF: the
//     reading of an idle bus with nothing driving MISO. It matches the
//     "init succeeds, no device present" shape the other stubbed
//     peripherals use.
//   - Timing is not modelled. `SPISettings` clock is recorded, never
//     honored; a transfer returns as fast as the host can run.

// SPI modes as an enum, ArduinoCore-API's `SPIMode`, for the same reason
// `BitOrder` is one (see Arduino.h): API-style code declares `SPIMode`
// parameters and passes SPI_MODE0 into them. The values are arduino-esp32's
// (0..3), the enumerators convert to `uint8_t` wherever the ESP32 spelling
// takes one, and the macros stay defined to themselves for `#ifdef`.
#if !defined(SPI_MODE0) && !defined(SPI_MODE1) && !defined(SPI_MODE2) && !defined(SPI_MODE3)
typedef enum {
    SPI_MODE0 = 0,
    SPI_MODE1 = 1,
    SPI_MODE2 = 2,
    SPI_MODE3 = 3,
} SPIMode;
#define SPI_MODE0 SPI_MODE0
#define SPI_MODE1 SPI_MODE1
#define SPI_MODE2 SPI_MODE2
#define SPI_MODE3 SPI_MODE3
#else
typedef uint8_t SPIMode;
#endif

// Feature and divider macros. SPI_HAS_TRANSACTION tells a driver that
// beginTransaction / endTransaction exist. The SPI_CLOCK_DIVn values are
// arduino-esp32's register encodings; setClockDivider records whatever it
// is given, so they matter only as the value a test reads back.
#ifndef SPI_HAS_TRANSACTION
#define SPI_HAS_TRANSACTION 1
#endif
#ifndef SPI_CLOCK_DIV2
#define SPI_CLOCK_DIV2 0x00101001
#define SPI_CLOCK_DIV4 0x00241001
#define SPI_CLOCK_DIV8 0x004c1001
#define SPI_CLOCK_DIV16 0x009c1001
#define SPI_CLOCK_DIV32 0x013c1001
#define SPI_CLOCK_DIV64 0x027c1001
#define SPI_CLOCK_DIV128 0x04fc1001
#endif
#ifndef SPI_INTERFACES_COUNT
#define SPI_INTERFACES_COUNT 1
#endif

#ifndef SPI_LSBFIRST
#define SPI_LSBFIRST LSBFIRST
#endif
#ifndef SPI_MSBFIRST
#define SPI_MSBFIRST MSBFIRST
#endif

// arduino-esp32 bus ids, with the classic ESP32 values. Accepted and
// recorded, never acted on — there is one host "bus" and it goes to
// whatever hook is registered.
#ifndef FSPI
#define FSPI 1
#endif
#ifndef HSPI
#define HSPI 2
#endif
#ifndef VSPI
#define VSPI 3
#endif

class SPISettings {
public:
    SPISettings() : _clock(1000000), _bitOrder(SPI_MSBFIRST), _dataMode(SPI_MODE0) {}
    SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode)
        : _clock(clock), _bitOrder(bitOrder), _dataMode(dataMode)
    {
    }

    // Read these from a hook or a test. The underscored fields below are
    // the same values; these accessors exist so observing code does not
    // have to look like it is reaching into private state.
    uint32_t clock() const { return _clock; }
    uint8_t bitOrder() const { return _bitOrder; }
    uint8_t dataMode() const { return _dataMode; }

    // ArduinoCore-API accessors, for code written against
    // `arduino::SPISettings` (a software SPI reads its settings this way).
    uint32_t getClockFreq() const { return _clock; }
    BitOrder getBitOrder() const { return static_cast<BitOrder>(_bitOrder); }
    SPIMode getDataMode() const { return static_cast<SPIMode>(_dataMode); }

    bool operator==(const SPISettings &rhs) const
    {
        return _clock == rhs._clock && _bitOrder == rhs._bitOrder && _dataMode == rhs._dataMode;
    }
    bool operator!=(const SPISettings &rhs) const { return !(*this == rhs); }

    // Public and underscored, matching arduino-esp32 — ESP32 sketches and
    // libraries write `settings._clock` directly, so the spelling has to
    // stay available.
    uint32_t _clock;
    uint8_t _bitOrder;
    uint8_t _dataMode;
};

// ArduinoCore-API's SPI base class. arduino-esp32 has none, but a driver
// that takes "any SPI bus" as `HardwareSPI &` (and a software SPI that
// derives from it) needs it to exist. The members are the API's; the
// settings type is this core's SPISettings, which is also what
// `arduino::SPISettings` names. Reachable as `arduino::HardwareSPI` too.
class HardwareSPI {
public:
    virtual ~HardwareSPI() {}

    virtual uint8_t transfer(uint8_t data) = 0;
    virtual uint16_t transfer16(uint16_t data) = 0;
    virtual void transfer(void *buf, size_t count) = 0;

    virtual void usingInterrupt(int interruptNumber) = 0;
    virtual void notUsingInterrupt(int interruptNumber) = 0;
    virtual void beginTransaction(SPISettings settings) = 0;
    virtual void endTransaction(void) = 0;

    virtual void attachInterrupt() = 0;
    virtual void detachInterrupt() = 0;

    virtual void begin() = 0;
    virtual void end() = 0;
};

namespace arduino {
using ::HardwareSPI;
using ::SPIMode;
using ::SPISettings;
} // namespace arduino

class SPIClass : public HardwareSPI {
public:
    // Called once per transferred byte. `out` is what the sketch pushed
    // (MOSI); the return value is what the sketch reads back (MISO). A
    // write-only device model returns anything — TinyGFX-style callers
    // ignore it.
    using TransferHook = uint8_t (*)(uint8_t out, void *user);

    // Called on `beginTransaction` with `active == true` and on
    // `endTransaction` with `active == false`. `settings` is the active
    // configuration in both cases.
    using TransactionHook = void (*)(bool active, const SPISettings &settings, void *user);

    // Bus setup, in the order the sketch performed it. `kConfig` covers
    // `setFrequency` / `setBitOrder` / `setDataMode` / `setClockDivider` /
    // `setHwCs` — the transaction-free spelling a driver reaches for when
    // it owns the bus outright. Read the new value off `settings()` (or
    // `getClockDivider()` / `hwCs()`) inside the hook.
    enum LifecycleEvent : uint8_t {
        kBegin = 0,
        kEnd,
        kConfig,
    };

    // Called after the change has been applied, so `spi` already reports
    // the new state. The whole object is passed rather than the changed
    // field: `bus()` is what tells two SPIClass instances apart when one
    // model watches both.
    using LifecycleHook = void (*)(LifecycleEvent event, const SPIClass &spi, void *user);

    explicit SPIClass(uint8_t spi_bus = HSPI) : _bus(spi_bus) {}

    // --- Arduino / ESP32 surface -------------------------------------

    // `begin()` is split from the pin-taking form only so it can be the
    // base class's override; the calls a sketch writes mean what they did.
    // A pin left at -1 takes what `setPins` stored, if it was called.
    void begin() override { begin(-1, -1, -1, -1); }
    void begin(int8_t sck, int8_t miso = -1, int8_t mosi = -1, int8_t ss = -1);
    void end() override;

    // Store the pins for a later `begin()`, ArduinoCore-API / RP2040 style.
    // Reported as kConfig; nothing is begun.
    void setPins(int8_t sck, int8_t miso, int8_t mosi, int8_t ss = -1)
    {
        _sck = sck;
        _miso = miso;
        _mosi = mosi;
        _ss = ss;
        _pinsPreset = true;
        reportLifecycle(kConfig);
    }

    void beginTransaction(SPISettings settings = SPISettings()) override;
    void endTransaction() override;

    void setBitOrder(uint8_t bitOrder)
    {
        _settings._bitOrder = bitOrder;
        reportLifecycle(kConfig);
    }

    void setDataMode(uint8_t dataMode)
    {
        _settings._dataMode = dataMode;
        reportLifecycle(kConfig);
    }

    void setFrequency(uint32_t frequency)
    {
        _settings._clock = frequency;
        reportLifecycle(kConfig);
    }

    void setClockDivider(uint32_t divider)
    {
        _clockDivider = divider;
        reportLifecycle(kConfig);
    }

    uint32_t getClockDivider() const { return _clockDivider; }

    void setHwCs(bool use)
    {
        _hwCs = use;
        reportLifecycle(kConfig);
    }

    uint8_t bus() const { return _bus; }

    // Hot path: one counter bump plus one null-checked indirect call.
    uint8_t transfer(uint8_t data) override
    {
        ++_transferCount;
        if (_transferHook) {
            return _transferHook(data, _transferHookUser);
        }
        return 0xFF;
    }

    uint16_t transfer16(uint16_t data) override
    {
        if (_settings._bitOrder == SPI_MSBFIRST) {
            const uint8_t hi = transfer(static_cast<uint8_t>(data >> 8));
            const uint8_t lo = transfer(static_cast<uint8_t>(data & 0xFF));
            return static_cast<uint16_t>(static_cast<uint16_t>(hi) << 8 | lo);
        }
        const uint8_t lo = transfer(static_cast<uint8_t>(data & 0xFF));
        const uint8_t hi = transfer(static_cast<uint8_t>(data >> 8));
        return static_cast<uint16_t>(static_cast<uint16_t>(hi) << 8 | lo);
    }

    uint32_t transfer32(uint32_t data);

    // Covers both the AVR `transfer(void *, size_t)` and the ESP32
    // `transfer(uint8_t *, uint32_t)` spellings; the buffer is updated in
    // place with what the hook returned.
    void transfer(void *data, size_t size) override;

    void transferBytes(const uint8_t *data, uint8_t *out, size_t size);
    void transferBits(uint32_t data, uint32_t *out, uint8_t bits);

    void write(uint8_t data) { (void)transfer(data); }
    void write16(uint16_t data) { (void)transfer16(data); }
    void write32(uint32_t data) { (void)transfer32(data); }
    void writeBytes(const uint8_t *data, size_t size);
    // On the host this is exactly writeBytes: there is no DMA and no
    // pixel byte swapping, so the model sees the bytes as the sketch
    // laid them out in memory.
    void writePixels(const void *data, size_t size) { writeBytes(static_cast<const uint8_t *>(data), size); }
    void writePattern(const uint8_t *data, uint8_t size, uint32_t repeat);

    // AVR interrupt-coordination surface. No interrupts on the host, so
    // these exist to keep sketches compiling.
    void usingInterrupt(int) override {}
    void notUsingInterrupt(int) override {}
    void attachInterrupt() override {}
    void detachInterrupt() override {}

    // --- Observation port -------------------------------------------

    void setTransferHook(TransferHook hook, void *user = nullptr)
    {
        _transferHook = hook;
        _transferHookUser = user;
    }

    void setTransactionHook(TransactionHook hook, void *user = nullptr)
    {
        _transactionHook = hook;
        _transactionHookUser = user;
    }

    void setLifecycleHook(LifecycleHook hook, void *user = nullptr)
    {
        _lifecycleHook = hook;
        _lifecycleHookUser = user;
    }

    void clearHooks()
    {
        _transferHook = nullptr;
        _transferHookUser = nullptr;
        _transactionHook = nullptr;
        _transactionHookUser = nullptr;
        _lifecycleHook = nullptr;
        _lifecycleHookUser = nullptr;
    }

    // Settings in force now: what the last `beginTransaction` installed,
    // or what `setFrequency` / `setBitOrder` / `setDataMode` left behind.
    const SPISettings &settings() const { return _settings; }
    bool inTransaction() const { return _inTransaction; }
    bool begun() const { return _begun; }
    bool hwCs() const { return _hwCs; }

    // Pins as `begin()` received them, -1 when defaulted. Lets a test
    // assert the wiring a sketch chose without a hook.
    int8_t sck() const { return _sck; }
    int8_t miso() const { return _miso; }
    int8_t mosi() const { return _mosi; }
    int8_t ss() const { return _ss; }

    // Bytes handed to `transfer` since the last reset — a device model is
    // not needed just to count traffic.
    uint32_t transferCount() const { return _transferCount; }
    void resetTransferCount() { _transferCount = 0; }

private:
    uint8_t _bus;
    SPISettings _settings;
    uint32_t _clockDivider = 0;
    uint32_t _transferCount = 0;
    int8_t _sck = -1;
    int8_t _miso = -1;
    int8_t _mosi = -1;
    int8_t _ss = -1;
    bool _begun = false;
    bool _pinsPreset = false;
    bool _hwCs = false;
    bool _inTransaction = false;
    TransferHook _transferHook = nullptr;
    void *_transferHookUser = nullptr;
    TransactionHook _transactionHook = nullptr;
    void *_transactionHookUser = nullptr;
    LifecycleHook _lifecycleHook = nullptr;
    void *_lifecycleHookUser = nullptr;

    void reportLifecycle(LifecycleEvent event)
    {
        if (_lifecycleHook) {
            _lifecycleHook(event, *this, _lifecycleHookUser);
        }
    }
};

extern SPIClass SPI;

#endif
