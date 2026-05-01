#ifndef HOST_ARDUINO_PRINT_H
#define HOST_ARDUINO_PRINT_H

#include <stddef.h>
#include <stdint.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <vector>

#include "pgmspace.h"
#include "WString.h"

#ifndef DEC
#define DEC 10
#endif
#ifndef HEX
#define HEX 16
#endif
#ifndef OCT
#define OCT 8
#endif
#ifndef BIN
#define BIN 2
#endif

class Print;

class Printable {
public:
    virtual ~Printable() {}
    virtual size_t printTo(Print &p) const = 0;
};

class Print {
public:
    virtual ~Print() {}
    virtual size_t write(uint8_t value) = 0;

    virtual size_t write(const uint8_t *buffer, size_t size)
    {
        size_t written = 0;
        while (size--) {
            written += write(*buffer++);
        }
        return written;
    }
    size_t write(const char *str)
    {
        return str ? write(reinterpret_cast<const uint8_t *>(str), std::strlen(str)) : 0;
    }
    size_t write(const char *buffer, size_t size)
    {
        return write(reinterpret_cast<const uint8_t *>(buffer), size);
    }

    size_t print(const String &value) { return write(value.c_str()); }
    size_t print(const char *value) { return write(value); }
    size_t print(const __FlashStringHelper *value) { return print(reinterpret_cast<const char *>(value)); }
    size_t print(char value) { return write(static_cast<uint8_t>(value)); }
    size_t print(bool value) { return print(value ? 1 : 0); }
    size_t print(unsigned char value, int base = DEC) { return printNumber(value, base); }
    size_t print(int value, int base = DEC) { return printSigned(value, base); }
    size_t print(unsigned int value, int base = DEC) { return printNumber(value, base); }
    size_t print(long value, int base = DEC) { return printSigned(value, base); }
    size_t print(unsigned long value, int base = DEC) { return printNumber(value, base); }
    size_t print(long long value, int base = DEC) { return printSigned(value, base); }
    size_t print(unsigned long long value, int base = DEC) { return printNumber(value, base); }
    size_t print(double value, int digits = 2) { return print(String(value, static_cast<unsigned char>(digits))); }
    size_t print(const Printable &value) { return value.printTo(*this); }

    size_t println() { return write("\r\n"); }
    size_t println(const String &value) { return print(value) + println(); }
    size_t println(const char *value) { return print(value) + println(); }
    size_t println(const __FlashStringHelper *value) { return print(value) + println(); }
    size_t println(char value) { return print(value) + println(); }
    size_t println(bool value) { return print(value) + println(); }
    size_t println(unsigned char value, int base = DEC) { return print(value, base) + println(); }
    size_t println(int value, int base = DEC) { return print(value, base) + println(); }
    size_t println(unsigned int value, int base = DEC) { return print(value, base) + println(); }
    size_t println(long value, int base = DEC) { return print(value, base) + println(); }
    size_t println(unsigned long value, int base = DEC) { return print(value, base) + println(); }
    size_t println(long long value, int base = DEC) { return print(value, base) + println(); }
    size_t println(unsigned long long value, int base = DEC) { return print(value, base) + println(); }
    size_t println(double value, int digits = 2) { return print(value, digits) + println(); }
    size_t println(const Printable &value) { return print(value) + println(); }

    size_t printf(const char *format, ...)
    {
        if (!format) {
            return 0;
        }
        va_list args;
        va_start(args, format);
        va_list copy;
        va_copy(copy, args);
        const int len = std::vsnprintf(NULL, 0, format, copy);
        va_end(copy);
        if (len <= 0) {
            va_end(args);
            return 0;
        }
        std::vector<char> buffer(static_cast<size_t>(len) + 1);
        std::vsnprintf(buffer.data(), buffer.size(), format, args);
        va_end(args);
        return write(buffer.data(), static_cast<size_t>(len));
    }

protected:
    void setWriteError(int err = 1) { write_error_ = err; }

public:
    int getWriteError() const { return write_error_; }
    void clearWriteError() { write_error_ = 0; }

private:
    int write_error_ = 0;

    size_t printNumber(unsigned long long value, int base)
    {
        return print(String(value, static_cast<unsigned char>(base)));
    }
    size_t printSigned(long long value, int base)
    {
        return print(String(value, static_cast<unsigned char>(base)));
    }
};

#endif
