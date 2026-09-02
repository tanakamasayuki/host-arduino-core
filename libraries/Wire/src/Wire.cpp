#include "Wire.h"

TwoWire Wire(0);
TwoWire Wire1(1);

bool TwoWire::begin(int sda, int scl, uint32_t frequency)
{
    _sda = sda;
    _scl = scl;
    if (frequency) {
        _frequency = frequency;
    }
    _begun = true;
    reportLifecycle(kBegin);
    return true;
}

bool TwoWire::begin(uint8_t address, int sda, int scl, uint32_t frequency)
{
    // Slave mode. Accepted so ESP32 sketches build; there is no master on
    // the other side of a host bus, so nothing ever addresses us.
    _address = address;
    return begin(sda, scl, frequency);
}

bool TwoWire::end()
{
    _begun = false;
    _transmitting = false;
    _txLength = 0;
    _rxLength = 0;
    _rxIndex = 0;
    reportLifecycle(kEnd);
    return true;
}

bool TwoWire::setPins(int sda, int scl)
{
    _sda = sda;
    _scl = scl;
    reportLifecycle(kSetPins);
    return true;
}

bool TwoWire::setClock(uint32_t frequency)
{
    _frequency = frequency;
    reportLifecycle(kSetClock);
    return true;
}

void TwoWire::setTimeOut(uint16_t timeout_ms)
{
    _timeout = timeout_ms;
    reportLifecycle(kSetTimeout);
}

void TwoWire::beginTransmission(uint16_t address)
{
    _address = address;
    _transmitting = true;
    _txLength = 0;
    _txOverflow = false;
}

uint8_t TwoWire::endTransmission(bool sendStop)
{
    if (!_transmitting) {
        return 4; // other error — no matching beginTransmission
    }
    _transmitting = false;
    ++_writeCount;

    if (_txOverflow) {
        _txLength = 0;
        return 1; // data too long for the transmit buffer
    }

    uint8_t status = 2; // address NACK: nothing on the bus by default
    if (_writeHook) {
        status = _writeHook(static_cast<uint8_t>(_address), _txBuffer, _txLength, sendStop, _writeHookUser);
    }
    _txLength = 0;
    return status;
}

size_t TwoWire::requestFrom(uint16_t address, size_t size, bool sendStop)
{
    _address = address;
    ++_readCount;
    _rxLength = 0;
    _rxIndex = 0;

    if (size > sizeof(_rxBuffer)) {
        size = sizeof(_rxBuffer);
    }
    if (_readHook && size) {
        const size_t supplied = _readHook(static_cast<uint8_t>(address), _rxBuffer, size, sendStop, _readHookUser);
        _rxLength = supplied > size ? size : supplied;
    }
    return _rxLength;
}

uint8_t TwoWire::requestFrom(uint16_t address, uint8_t size, bool sendStop)
{
    return static_cast<uint8_t>(requestFrom(address, static_cast<size_t>(size), sendStop));
}

uint8_t TwoWire::requestFrom(uint16_t address, uint8_t size)
{
    return requestFrom(address, size, true);
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t size, bool sendStop)
{
    return requestFrom(static_cast<uint16_t>(address), size, sendStop);
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t size)
{
    return requestFrom(static_cast<uint16_t>(address), size, true);
}

uint8_t TwoWire::requestFrom(int address, int size, int sendStop)
{
    return requestFrom(static_cast<uint16_t>(address), static_cast<uint8_t>(size), sendStop != 0);
}

uint8_t TwoWire::requestFrom(int address, int size)
{
    return requestFrom(static_cast<uint16_t>(address), static_cast<uint8_t>(size), true);
}

size_t TwoWire::write(uint8_t data)
{
    if (!_transmitting) {
        return 0;
    }
    if (_txLength >= sizeof(_txBuffer)) {
        _txOverflow = true;
        return 0;
    }
    _txBuffer[_txLength++] = data;
    return 1;
}

size_t TwoWire::write(const uint8_t *data, size_t size)
{
    if (!_transmitting || !data) {
        return 0;
    }
    size_t written = 0;
    for (size_t i = 0; i < size; ++i) {
        if (write(data[i]) != 1) {
            break;
        }
        ++written;
    }
    return written;
}

int TwoWire::available()
{
    return static_cast<int>(_rxLength - _rxIndex);
}

int TwoWire::read()
{
    if (_rxIndex >= _rxLength) {
        return -1;
    }
    return _rxBuffer[_rxIndex++];
}

int TwoWire::peek()
{
    if (_rxIndex >= _rxLength) {
        return -1;
    }
    return _rxBuffer[_rxIndex];
}

void TwoWire::flush()
{
    _rxLength = 0;
    _rxIndex = 0;
    _txLength = 0;
}
