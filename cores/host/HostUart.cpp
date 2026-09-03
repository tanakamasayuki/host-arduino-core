#include "HostUart.h"

// arduino-esp32 numbers the device UARTs 1 and 2; `Serial` is UART 0 and
// lives elsewhere (HostRuntime.cpp), being the console rather than a bus.
HostUart Serial1(1);
HostUart Serial2(2);

void HostUart::begin(unsigned long baud, uint32_t config, int8_t rxPin, int8_t txPin)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _baud = baud;
    _config = config;
    _rxPin = rxPin;
    _txPin = txPin;
    _begun = true;
    // Whatever was in flight belongs to the previous session. A driver
    // that re-begins mid-test gets a clean conversation, which is what
    // re-initializing a real UART amounts to.
    _tx.clear();
    _rx.clear();
}

void HostUart::end()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _begun = false;
    _tx.clear();
    _rx.clear();
}

void HostUart::setPins(int8_t rxPin, int8_t txPin)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _rxPin = rxPin;
    _txPin = txPin;
}

void HostUart::updateBaudRate(unsigned long baud)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _baud = baud;
}

int HostUart::availableForWrite()
{
    std::lock_guard<std::mutex> lock(_mutex);
    return static_cast<int>(_tx.size() >= _txLimit ? 0 : _txLimit - _tx.size());
}

size_t HostUart::write(uint8_t value)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_tx.size() >= _txLimit) {
        // Drop the newest rather than the oldest. Losing the tail of a
        // command is easier to recognize than losing its head, and the
        // sticky flag is what a test actually checks.
        _txOverflow = true;
        return 0;
    }
    _tx.push_back(value);
    ++_txTotal;
    return 1;
}

size_t HostUart::write(const uint8_t *buffer, size_t size)
{
    if (!buffer) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(_mutex);
    size_t written = 0;
    for (size_t i = 0; i < size; ++i) {
        if (_tx.size() >= _txLimit) {
            _txOverflow = true;
            break;
        }
        _tx.push_back(buffer[i]);
        ++_txTotal;
        ++written;
    }
    return written;
}

int HostUart::available()
{
    std::lock_guard<std::mutex> lock(_mutex);
    return static_cast<int>(_rx.size());
}

int HostUart::read()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_rx.empty()) {
        return -1;
    }
    const uint8_t value = _rx.front();
    _rx.pop_front();
    return static_cast<int>(value);
}

int HostUart::peek()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_rx.empty()) {
        return -1;
    }
    return static_cast<int>(_rx.front());
}

void HostUart::flush()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _rx.clear();
}

void HostUart::setRxBufferSize(size_t size)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _rxLimit = size ? size : 1;
    while (_rx.size() > _rxLimit) {
        _rx.pop_back();
        _rxOverflow = true;
    }
}

void HostUart::setTxBufferSize(size_t size)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _txLimit = size ? size : 1;
    while (_tx.size() > _txLimit) {
        _tx.pop_back();
        _txOverflow = true;
    }
}

size_t HostUart::txAvailable()
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _tx.size();
}

size_t HostUart::readTx(uint8_t *buffer, size_t size)
{
    if (!buffer || size == 0) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(_mutex);
    size_t taken = 0;
    while (taken < size && !_tx.empty()) {
        buffer[taken++] = _tx.front();
        _tx.pop_front();
    }
    return taken;
}

String HostUart::readTxString()
{
    std::lock_guard<std::mutex> lock(_mutex);
    String out;
    out.reserve(_tx.size());
    while (!_tx.empty()) {
        out += static_cast<char>(_tx.front());
        _tx.pop_front();
    }
    return out;
}

void HostUart::clearTx()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _tx.clear();
}

size_t HostUart::pushRx(const uint8_t *buffer, size_t size)
{
    if (!buffer) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(_mutex);
    size_t pushed = 0;
    for (size_t i = 0; i < size; ++i) {
        if (_rx.size() >= _rxLimit) {
            _rxOverflow = true;
            break;
        }
        _rx.push_back(buffer[i]);
        ++_rxTotal;
        ++pushed;
    }
    return pushed;
}

size_t HostUart::pushRx(const char *text)
{
    if (!text) {
        return 0;
    }
    size_t len = 0;
    while (text[len] != '\0') {
        ++len;
    }
    return pushRx(reinterpret_cast<const uint8_t *>(text), len);
}

void HostUart::clearRx()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _rx.clear();
}

void HostUart::clearOverflow()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _txOverflow = false;
    _rxOverflow = false;
}

void HostUart::resetTotals()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _txTotal = 0;
    _rxTotal = 0;
}
