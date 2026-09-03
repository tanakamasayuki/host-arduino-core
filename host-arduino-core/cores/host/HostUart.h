#ifndef HOST_ARDUINO_UART_H
#define HOST_ARDUINO_UART_H

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <mutex>

#include "Stream.h"

// Device-facing UARTs — `Serial1`, `Serial2`.
//
// `Serial` is the console: it goes out over the TCP runtime (or stdout on
// the `display` board) and is how a test reads what the sketch printed.
// This class is the other kind of UART, the one a device hangs off — a
// GPS, a modem, a scale — where the interesting traffic is a conversation
// between the sketch and something a test is pretending to be.
//
// So it is not wired to anything outside the process. Both directions are
// plain in-memory queues that program code drives:
//
//   sketch --write()--> tx queue --readTx()--> driver
//   sketch <--read()--- rx queue <--pushRx()-- driver
//
// A driver reads the command out of the tx queue, works out the answer,
// and pushes it into the rx queue. Because nothing else consumes either
// queue, a test owns the whole conversation.
//
//   Serial1.begin(9600);
//   ...
//   // in the driver, typically from kPreLoop:
//   uint8_t cmd[64];
//   const size_t n = Serial1.readTx(cmd, sizeof(cmd));
//   if (n) Serial1.pushRx("$GPGGA,...\r\n");
//
// Answering inside one iteration. A sketch that writes a command and
// reads the reply before returning from `loop()` — every AT-command
// driver does this — cannot be served from `kPreLoop`, because the reply
// would arrive an iteration too late. It is serviceable through the clock
// port instead: `Stream::readBytes` waits via
// `HostArduino::clockWaitMicros`, so a driver that has overridden the
// wait can drain tx and push rx from inside the sketch's own blocking
// read. See cores/host/HostClock.h.
//
// Not `HardwareSerial`. On real silicon `Serial`, `Serial1`, and USB CDC
// are all one class, but that class exists to describe a peripheral this
// core does not have, and taking a `HardwareSerial&` is a poor way for a
// library to ask for "somewhere to talk" in the first place — `Stream&`
// says it without the baggage. So this derives from `Stream` and nothing
// else, and `HardwareSerial` stays what it was, an alias for the console
// class. A library that insists on `HardwareSerial&` will not accept
// `Serial1` here; if a real sketch needs that, say so and it can be
// revisited.
//
// Not modelled: baud timing (bytes appear the instant they are written),
// framing, parity, break detection, flow control. `begin()` records what
// it was given so a test can assert the wiring, and nothing enforces it.

// arduino-esp32's `SerialConfig` values, verbatim, so a sketch shared with
// real silicon compiles and prints the same numbers. Guarded by a macro
// rather than by `SERIAL_8N1` itself, which is an enumerator and so
// invisible to the preprocessor.
#ifndef HOST_ARDUINO_SERIAL_CONFIG_DEFINED
#define HOST_ARDUINO_SERIAL_CONFIG_DEFINED
enum SerialConfig {
    SERIAL_5N1 = 0x8000010,
    SERIAL_6N1 = 0x8000014,
    SERIAL_7N1 = 0x8000018,
    SERIAL_8N1 = 0x800001c,
    SERIAL_5N2 = 0x8000030,
    SERIAL_6N2 = 0x8000034,
    SERIAL_7N2 = 0x8000038,
    SERIAL_8N2 = 0x800003c,
    SERIAL_5E1 = 0x8000012,
    SERIAL_6E1 = 0x8000016,
    SERIAL_7E1 = 0x800001a,
    SERIAL_8E1 = 0x800001e,
    SERIAL_5E2 = 0x8000032,
    SERIAL_6E2 = 0x8000036,
    SERIAL_7E2 = 0x800003a,
    SERIAL_8E2 = 0x800003e,
    SERIAL_5O1 = 0x8000013,
    SERIAL_6O1 = 0x8000017,
    SERIAL_7O1 = 0x800001b,
    SERIAL_8O1 = 0x800001f,
    SERIAL_5O2 = 0x8000033,
    SERIAL_6O2 = 0x8000037,
    SERIAL_7O2 = 0x800003b,
    SERIAL_8O2 = 0x800003f
};
#endif

class HostUart : public Stream {
public:
    // Bytes each direction holds before it starts dropping. Small on
    // purpose: a driver that is not draining should find out, not
    // accumulate a megabyte of unread commands.
    static constexpr size_t kDefaultBufferSize = 1024;

    explicit HostUart(uint8_t uart_num = 1) : _uart(uart_num) {}

    // --- Arduino / ESP32 surface -------------------------------------

    void begin(unsigned long baud, uint32_t config = SERIAL_8N1, int8_t rxPin = -1, int8_t txPin = -1);
    void end();

    void setPins(int8_t rxPin, int8_t txPin);
    void updateBaudRate(unsigned long baud);
    unsigned long baudRate() const { return _baud; }

    // Bytes that would be accepted by `write` right now. A sketch that
    // respects it never overflows the tx queue.
    int availableForWrite();

    size_t write(uint8_t value) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    using Print::write;

    int available() override;
    int read() override;
    int peek() override;

    // Drops whatever the sketch has not read yet, matching how
    // arduino-esp32 treats the receive side. The tx queue is left alone —
    // it is the driver's to consume, and discarding it here would lose a
    // command the sketch believes it sent.
    void flush() override;

    // True once `begin()` has been called, like `Serial`'s.
    operator bool() const { return _begun; }

    void setRxBufferSize(size_t size);
    void setTxBufferSize(size_t size);

    // --- Driver side -------------------------------------------------

    // What `begin()` was given, -1 for a pin left defaulted.
    bool begun() const { return _begun; }
    uint32_t config() const { return _config; }
    int8_t rxPin() const { return _rxPin; }
    int8_t txPin() const { return _txPin; }
    uint8_t uartNum() const { return _uart; }

    // Bytes the sketch has written and nobody has taken yet.
    size_t txAvailable();

    // Take up to `size` of them, removing what it returns. This is the
    // read direction for a driver: whatever comes back is what the sketch
    // put on the wire, in order.
    size_t readTx(uint8_t *buffer, size_t size);

    // Same, as a `String`, for the line-oriented protocols (NMEA, AT)
    // where that is what a driver wants to match against.
    String readTxString();

    void clearTx();

    // Push bytes the sketch will read back. The response direction, the
    // counterpart of `HostArduino::setPinValue` on the GPIO port.
    // Returns how many were accepted — short of `size` means the rx queue
    // filled up.
    size_t pushRx(const uint8_t *buffer, size_t size);
    size_t pushRx(const char *text);
    size_t pushRx(uint8_t value) { return pushRx(&value, 1); }

    void clearRx();

    // Set when a queue had to drop bytes: tx because the driver did not
    // drain, rx because the sketch did not read. Sticky until cleared, so
    // a test can check once at the end instead of after every push.
    bool txOverflowed() const { return _txOverflow; }
    bool rxOverflowed() const { return _rxOverflow; }
    void clearOverflow();

    // Totals since the last reset, whether or not anything drained them.
    // Enough to assert a sketch is talking at all without decoding what
    // it said.
    uint32_t txTotal() const { return _txTotal; }
    uint32_t rxTotal() const { return _rxTotal; }
    void resetTotals();

private:
    uint8_t _uart;
    bool _begun = false;
    unsigned long _baud = 0;
    uint32_t _config = SERIAL_8N1;
    int8_t _rxPin = -1;
    int8_t _txPin = -1;

    // The sketch writes on its own thread (`loop`, or a FreeRTOS task)
    // while a driver drains from a lifecycle hook or from inside the
    // clock port's wait. That is two threads on one queue, so unlike the
    // GPIO port — which is inline on a path taken millions of times per
    // frame and cannot afford a lock — these take one. UART traffic is
    // slow enough that it does not matter.
    //
    // The lock covers the queues and the limits. The scalars `begin()`
    // records are read without it, the same way `SPI.sck()` and
    // `Wire.sda()` are: a driver reads them after the sketch has set them
    // up, and locking a `begun()` check would be ceremony without a case
    // behind it.
    mutable std::mutex _mutex;
    std::deque<uint8_t> _tx;
    std::deque<uint8_t> _rx;
    size_t _txLimit = kDefaultBufferSize;
    size_t _rxLimit = kDefaultBufferSize;
    bool _txOverflow = false;
    bool _rxOverflow = false;
    uint32_t _txTotal = 0;
    uint32_t _rxTotal = 0;
};

extern HostUart Serial1;
extern HostUart Serial2;

#endif
