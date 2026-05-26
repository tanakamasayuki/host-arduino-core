#ifndef HOST_ARDUINO_CLIENT_H
#define HOST_ARDUINO_CLIENT_H

#include <stdint.h>
#include <stddef.h>

#include "IPAddress.h"
#include "Stream.h"

class Client : public Stream
{
public:
    virtual int connect(IPAddress ip, uint16_t port) = 0;
    virtual int connect(const char *host, uint16_t port) = 0;
    virtual size_t write(uint8_t) override = 0;
    virtual size_t write(const uint8_t *buf, size_t size) override = 0;
    virtual int available() override = 0;
    virtual int read() override = 0;
    virtual int read(uint8_t *buf, size_t size) = 0;
    virtual int peek() override = 0;
    virtual void flush() override = 0;
    virtual void stop() = 0;
    virtual uint8_t connected() = 0;
    virtual operator bool() = 0;
    using Print::write;

protected:
    uint8_t *rawIPAddress(IPAddress &addr) { return addr.raw_address(); }
};

#endif
