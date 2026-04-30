#ifndef HOST_ARDUINO_RUNTIME_H
#define HOST_ARDUINO_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#include <chrono>
#include <sstream>
#include <string>
#include <thread>

namespace HostArduino {

bool runtimeStart(int argc, char **argv);
void runtimeStop();
bool runtimeShouldStop();
void runtimePoll();
size_t serialWrite(const char *data, size_t len);
int serialAvailable();
int serialRead();

} // namespace HostArduino

class SerialClass {
public:
    void begin(unsigned long) {}
    void end() {}

    template <typename T>
    void print(const T &value)
    {
        std::ostringstream oss;
        oss << value;
        const std::string s = oss.str();
        HostArduino::serialWrite(s.data(), s.size());
    }

    void print(const char *value)
    {
        if (!value) {
            return;
        }
        const std::string s(value);
        HostArduino::serialWrite(s.data(), s.size());
    }

    void print(char value)
    {
        HostArduino::serialWrite(&value, 1);
    }

    template <typename T>
    void println(const T &value)
    {
        print(value);
        println();
    }

    void println()
    {
        const char nl = '\n';
        HostArduino::serialWrite(&nl, 1);
    }

    size_t write(uint8_t value)
    {
        const char ch = static_cast<char>(value);
        return HostArduino::serialWrite(&ch, 1);
    }

    size_t write(const uint8_t *buffer, size_t size)
    {
        return HostArduino::serialWrite(reinterpret_cast<const char *>(buffer), size);
    }

    size_t write(const char *buffer, size_t size)
    {
        return HostArduino::serialWrite(buffer, size);
    }

    int available()
    {
        return HostArduino::serialAvailable();
    }

    int read()
    {
        return HostArduino::serialRead();
    }

    int availableForWrite()
    {
        return 1024;
    }

    void flush() {}

    operator bool() const
    {
        return true;
    }
};

extern SerialClass Serial;

inline uint32_t millis()
{
    static const auto start = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
}

inline uint32_t micros()
{
    static const auto start = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(now - start).count());
}

inline void delay(unsigned long ms)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (!HostArduino::runtimeShouldStop() && std::chrono::steady_clock::now() < end) {
        HostArduino::runtimePoll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

inline void delayMicroseconds(unsigned int us)
{
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

inline void yield()
{
    HostArduino::runtimePoll();
    std::this_thread::yield();
}

inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return 0; }
inline int analogRead(int) { return 0; }
inline void analogWrite(int, int) {}

#endif
