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

#ifndef SPI_MODE0
#define SPI_MODE0 0x00
#define SPI_MODE1 0x01
#define SPI_MODE2 0x02
#define SPI_MODE3 0x03
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

    // Public and underscored, matching arduino-esp32 — ESP32 sketches and
    // libraries write `settings._clock` directly, so the spelling has to
    // stay available.
    uint32_t _clock;
    uint8_t _bitOrder;
    uint8_t _dataMode;
};

class SPIClass {
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

    explicit SPIClass(uint8_t spi_bus = HSPI) : _bus(spi_bus) {}

    // --- Arduino / ESP32 surface -------------------------------------

    void begin(int8_t sck = -1, int8_t miso = -1, int8_t mosi = -1, int8_t ss = -1);
    void end();

    void beginTransaction(SPISettings settings = SPISettings());
    void endTransaction();

    void setBitOrder(uint8_t bitOrder) { _settings._bitOrder = bitOrder; }
    void setDataMode(uint8_t dataMode) { _settings._dataMode = dataMode; }
    void setFrequency(uint32_t frequency) { _settings._clock = frequency; }
    void setClockDivider(uint32_t divider) { _clockDivider = divider; }
    uint32_t getClockDivider() const { return _clockDivider; }
    void setHwCs(bool use) { _hwCs = use; }
    uint8_t bus() const { return _bus; }

    // Hot path: one counter bump plus one null-checked indirect call.
    uint8_t transfer(uint8_t data)
    {
        ++_transferCount;
        if (_transferHook) {
            return _transferHook(data, _transferHookUser);
        }
        return 0xFF;
    }

    uint16_t transfer16(uint16_t data)
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
    void transfer(void *data, size_t size);

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
    void usingInterrupt(int) {}
    void notUsingInterrupt(int) {}
    void attachInterrupt() {}
    void detachInterrupt() {}

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

    void clearHooks()
    {
        _transferHook = nullptr;
        _transferHookUser = nullptr;
        _transactionHook = nullptr;
        _transactionHookUser = nullptr;
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
    bool _hwCs = false;
    bool _inTransaction = false;
    TransferHook _transferHook = nullptr;
    void *_transferHookUser = nullptr;
    TransactionHook _transactionHook = nullptr;
    void *_transactionHookUser = nullptr;
};

extern SPIClass SPI;

#endif
