#ifndef HOST_ARDUINO_STREAM_H
#define HOST_ARDUINO_STREAM_H

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <chrono>
#include <thread>

#include "Print.h"

class Stream : public Print {
public:
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
    virtual void flush() {}

    void setTimeout(unsigned long timeout) { timeout_ = timeout; }
    unsigned long getTimeout() const { return timeout_; }

    int timedRead()
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_);
        do {
            const int c = read();
            if (c >= 0) {
                return c;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (std::chrono::steady_clock::now() < deadline);
        return -1;
    }

    int timedPeek()
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_);
        do {
            const int c = peek();
            if (c >= 0) {
                return c;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (std::chrono::steady_clock::now() < deadline);
        return -1;
    }

    size_t readBytes(char *buffer, size_t length)
    {
        size_t count = 0;
        while (count < length) {
            const int c = timedRead();
            if (c < 0) {
                break;
            }
            buffer[count++] = static_cast<char>(c);
        }
        return count;
    }
    size_t readBytes(uint8_t *buffer, size_t length)
    {
        return readBytes(reinterpret_cast<char *>(buffer), length);
    }
    size_t readBytesUntil(char terminator, char *buffer, size_t length)
    {
        if (!buffer || length == 0) {
            return 0;
        }
        size_t count = 0;
        while (count < length) {
            const int c = timedRead();
            if (c < 0 || c == terminator) {
                break;
            }
            buffer[count++] = static_cast<char>(c);
        }
        return count;
    }
    bool find(const char *target)
    {
        return findUntil(target, NULL);
    }
    bool find(char *target)
    {
        return find(static_cast<const char *>(target));
    }
    bool findUntil(const char *target, const char *terminator)
    {
        if (!target || !*target) {
            return true;
        }

        const size_t targetLen = strlen(target);
        const size_t terminatorLen = terminator ? strlen(terminator) : 0;
        size_t targetIndex = 0;
        size_t terminatorIndex = 0;

        while (true) {
            const int c = timedRead();
            if (c < 0) {
                return false;
            }

            if (terminatorLen > 0) {
                if (c == terminator[terminatorIndex]) {
                    ++terminatorIndex;
                    if (terminatorIndex == terminatorLen) {
                        return false;
                    }
                } else {
                    terminatorIndex = (c == terminator[0]) ? 1 : 0;
                }
            }

            if (c == target[targetIndex]) {
                ++targetIndex;
                if (targetIndex == targetLen) {
                    return true;
                }
            } else {
                targetIndex = (c == target[0]) ? 1 : 0;
            }
        }
    }
    bool findUntil(char *target, char *terminator)
    {
        return findUntil(static_cast<const char *>(target), static_cast<const char *>(terminator));
    }
    String readString()
    {
        String out;
        int c;
        while ((c = timedRead()) >= 0) {
            out += static_cast<char>(c);
        }
        return out;
    }
    String readStringUntil(char terminator)
    {
        String out;
        int c;
        while ((c = timedRead()) >= 0) {
            if (c == terminator) {
                break;
            }
            out += static_cast<char>(c);
        }
        return out;
    }
    long parseInt()
    {
        String token = readToken(true, -1);
        return token.toInt();
    }
    long parseInt(char skipChar)
    {
        String token = readToken(true, static_cast<unsigned char>(skipChar));
        return token.toInt();
    }
    float parseFloat()
    {
        String token = readToken(false, -1);
        return token.toFloat();
    }
    float parseFloat(char skipChar)
    {
        String token = readToken(false, static_cast<unsigned char>(skipChar));
        return token.toFloat();
    }

protected:
    unsigned long timeout_ = 1000;

private:
    String readToken(bool integerOnly, int skipChar)
    {
        String out;
        bool started = false;
        while (true) {
            const int c = timedPeek();
            if (c < 0) {
                break;
            }
            if (c == skipChar) {
                read();
                continue;
            }
            const bool valid = isdigit(c) || c == '-' || c == '+' || (!integerOnly && c == '.');
            if (!valid) {
                read();
                if (started) {
                    break;
                }
                continue;
            }
            started = true;
            out += static_cast<char>(read());
        }
        return out;
    }
};

#endif
