#ifndef HOST_ARDUINO_UDP_H
#define HOST_ARDUINO_UDP_H

#include <stdint.h>
#include <stddef.h>

#include "IPAddress.h"
#include "Stream.h"

class UDP : public Stream {
public:
    virtual uint8_t begin(uint16_t port) = 0;
    virtual uint8_t beginMulticast(IPAddress, uint16_t) { return 0; }
    virtual void stop() = 0;

    virtual int beginPacket(IPAddress ip, uint16_t port) = 0;
    virtual int beginPacket(const char *host, uint16_t port) = 0;
    virtual int endPacket() = 0;
    virtual size_t write(uint8_t) override = 0;
    virtual size_t write(const uint8_t *buffer, size_t size) override = 0;

    virtual int parsePacket() = 0;
    virtual int available() override = 0;
    virtual int read() override = 0;
    virtual int read(unsigned char *buffer, size_t len) = 0;
    virtual int read(char *buffer, size_t len) { return read((unsigned char *)buffer, len); }
    virtual int peek() override = 0;
    virtual void flush() override {}

    virtual IPAddress remoteIP() = 0;
    virtual uint16_t remotePort() = 0;

protected:
    uint8_t *rawIPAddress(IPAddress &addr) { return addr.raw_address(); }
};

#endif
