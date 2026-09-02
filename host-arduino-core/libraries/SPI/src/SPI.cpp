#include "SPI.h"

// arduino-esp32 spells the default instance this way too.
SPIClass SPI(FSPI);

void SPIClass::begin(int8_t sck, int8_t miso, int8_t mosi, int8_t ss)
{
    _sck = sck;
    _miso = miso;
    _mosi = mosi;
    _ss = ss;
    _begun = true;
    reportLifecycle(kBegin);
}

void SPIClass::end()
{
    _begun = false;
    _inTransaction = false;
    reportLifecycle(kEnd);
}

void SPIClass::beginTransaction(SPISettings settings)
{
    _settings = settings;
    _inTransaction = true;
    if (_transactionHook) {
        _transactionHook(true, _settings, _transactionHookUser);
    }
}

void SPIClass::endTransaction()
{
    _inTransaction = false;
    if (_transactionHook) {
        _transactionHook(false, _settings, _transactionHookUser);
    }
}

uint32_t SPIClass::transfer32(uint32_t data)
{
    uint32_t in = 0;
    if (_settings._bitOrder == SPI_MSBFIRST) {
        for (int shift = 24; shift >= 0; shift -= 8) {
            const uint8_t byte_in = transfer(static_cast<uint8_t>((data >> shift) & 0xFF));
            in |= static_cast<uint32_t>(byte_in) << shift;
        }
    } else {
        for (int shift = 0; shift <= 24; shift += 8) {
            const uint8_t byte_in = transfer(static_cast<uint8_t>((data >> shift) & 0xFF));
            in |= static_cast<uint32_t>(byte_in) << shift;
        }
    }
    return in;
}

void SPIClass::transfer(void *data, size_t size)
{
    uint8_t *buffer = static_cast<uint8_t *>(data);
    for (size_t i = 0; i < size; ++i) {
        buffer[i] = transfer(buffer[i]);
    }
}

void SPIClass::transferBytes(const uint8_t *data, uint8_t *out, size_t size)
{
    for (size_t i = 0; i < size; ++i) {
        // ESP32 allows a null `data` (read-only burst); it pushes zeros.
        const uint8_t sent = transfer(data ? data[i] : 0x00);
        if (out) {
            out[i] = sent;
        }
    }
}

void SPIClass::transferBits(uint32_t data, uint32_t *out, uint8_t bits)
{
    // Byte-granular approximation of the ESP32 call: the bits are sent in
    // whole bytes, most significant first for MSBFIRST. Anything not a
    // multiple of 8 is padded — the core has no wire to place partial
    // bytes on, and a device model watching bytes cannot see the
    // difference anyway.
    if (bits == 0 || bits > 32) {
        if (out) {
            *out = 0;
        }
        return;
    }
    const uint8_t count = static_cast<uint8_t>((bits + 7) / 8);
    uint32_t in = 0;
    if (_settings._bitOrder == SPI_MSBFIRST) {
        for (int i = count - 1; i >= 0; --i) {
            const int shift = i * 8;
            const uint8_t byte_in = transfer(static_cast<uint8_t>((data >> shift) & 0xFF));
            in |= static_cast<uint32_t>(byte_in) << shift;
        }
    } else {
        for (uint8_t i = 0; i < count; ++i) {
            const int shift = i * 8;
            const uint8_t byte_in = transfer(static_cast<uint8_t>((data >> shift) & 0xFF));
            in |= static_cast<uint32_t>(byte_in) << shift;
        }
    }
    if (out) {
        *out = in;
    }
}

void SPIClass::writeBytes(const uint8_t *data, size_t size)
{
    for (size_t i = 0; i < size; ++i) {
        (void)transfer(data ? data[i] : 0x00);
    }
}

void SPIClass::writePattern(const uint8_t *data, uint8_t size, uint32_t repeat)
{
    if (!data || size == 0) {
        return;
    }
    for (uint32_t r = 0; r < repeat; ++r) {
        writeBytes(data, size);
    }
}
