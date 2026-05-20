#ifndef HOST_ARDUINO_IPADDRESS_H
#define HOST_ARDUINO_IPADDRESS_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Print.h"
#include "Printable.h"
#include "WString.h"

class IPAddress : public Printable
{
public:
    IPAddress() { _address.dword = 0; }
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
    {
        _address.bytes[0] = a;
        _address.bytes[1] = b;
        _address.bytes[2] = c;
        _address.bytes[3] = d;
    }
    IPAddress(uint32_t address) { _address.dword = address; }
    IPAddress(const uint8_t *address) { memcpy(_address.bytes, address, 4); }

    bool fromString(const char *address)
    {
        unsigned int a, b, c, d;
        if (!address)
            return false;
        if (sscanf(address, "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
            return false;
        if (a > 255 || b > 255 || c > 255 || d > 255)
            return false;
        _address.bytes[0] = (uint8_t)a;
        _address.bytes[1] = (uint8_t)b;
        _address.bytes[2] = (uint8_t)c;
        _address.bytes[3] = (uint8_t)d;
        return true;
    }
    bool fromString(const String &address) { return fromString(address.c_str()); }

    operator uint32_t() const { return _address.dword; }
    bool operator==(const IPAddress &addr) const { return _address.dword == addr._address.dword; }
    bool operator!=(const IPAddress &addr) const { return !(*this == addr); }
    bool operator==(const uint8_t *addr) const { return memcmp(addr, _address.bytes, 4) == 0; }

    uint8_t operator[](int index) const { return _address.bytes[index]; }
    uint8_t &operator[](int index) { return _address.bytes[index]; }

    IPAddress &operator=(const uint8_t *address)
    {
        memcpy(_address.bytes, address, 4);
        return *this;
    }
    IPAddress &operator=(uint32_t address)
    {
        _address.dword = address;
        return *this;
    }

    size_t printTo(Print &p) const override
    {
        size_t n = 0;
        for (int i = 0; i < 4; ++i)
        {
            n += p.print((unsigned int)_address.bytes[i]);
            if (i < 3)
                n += p.print('.');
        }
        return n;
    }

    String toString() const
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
                 _address.bytes[0], _address.bytes[1],
                 _address.bytes[2], _address.bytes[3]);
        return String(buf);
    }

    uint8_t *raw_address() { return _address.bytes; }

private:
    union
    {
        uint8_t bytes[4];
        uint32_t dword;
    } _address;
};

const IPAddress INADDR_NONE(0, 0, 0, 0);

#endif
