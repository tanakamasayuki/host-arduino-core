#ifndef HOST_ARDUINO_WSTRING_H
#define HOST_ARDUINO_WSTRING_H

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>

class String {
public:
    String() {}
    String(const char *value) : value_(value ? value : "") {}
    String(const std::string &value) : value_(value) {}
    String(char value) : value_(1, value) {}
    String(unsigned char value, unsigned char base = 10) : value_(formatUnsigned(value, base)) {}
    String(int value, unsigned char base = 10) : value_(formatSigned(value, base)) {}
    String(unsigned int value, unsigned char base = 10) : value_(formatUnsigned(value, base)) {}
    String(long value, unsigned char base = 10) : value_(formatSigned(value, base)) {}
    String(unsigned long value, unsigned char base = 10) : value_(formatUnsigned(value, base)) {}
    String(long long value, unsigned char base = 10) : value_(formatSigned(value, base)) {}
    String(unsigned long long value, unsigned char base = 10) : value_(formatUnsigned(value, base)) {}
    String(float value, unsigned char decimalPlaces = 2) : value_(formatFloat(value, decimalPlaces)) {}
    String(double value, unsigned char decimalPlaces = 2) : value_(formatFloat(value, decimalPlaces)) {}

    const char *c_str() const { return value_.c_str(); }
    const char *begin() const { return value_.c_str(); }
    const char *end() const { return value_.c_str() + value_.size(); }
    char *begin() { return value_.empty() ? NULL : &value_[0]; }
    char *end() { return value_.empty() ? NULL : &value_[0] + value_.size(); }

    unsigned int length() const { return static_cast<unsigned int>(value_.size()); }
    bool isEmpty() const { return value_.empty(); }
    void reserve(unsigned int size) { value_.reserve(size); }
    void invalidate() { value_.clear(); }

    char charAt(unsigned int index) const { return index < value_.size() ? value_[index] : '\0'; }
    void setCharAt(unsigned int index, char c)
    {
        if (index < value_.size()) {
            value_[index] = c;
        }
    }
    char operator[](unsigned int index) const { return charAt(index); }
    char &operator[](unsigned int index) { return value_[index]; }

    bool concat(const String &other)
    {
        value_ += other.value_;
        return true;
    }
    bool concat(const char *other)
    {
        if (other) {
            value_ += other;
        }
        return true;
    }
    bool concat(char c)
    {
        value_ += c;
        return true;
    }
    bool concat(int value)
    {
        value_ += String(value).value_;
        return true;
    }
    bool concat(unsigned int value)
    {
        value_ += String(value).value_;
        return true;
    }
    bool concat(long value)
    {
        value_ += String(value).value_;
        return true;
    }
    bool concat(unsigned long value)
    {
        value_ += String(value).value_;
        return true;
    }
    bool concat(long long value)
    {
        value_ += String(value).value_;
        return true;
    }
    bool concat(unsigned long long value)
    {
        value_ += String(value).value_;
        return true;
    }
    bool concat(float value)
    {
        value_ += String(value).value_;
        return true;
    }
    bool concat(double value)
    {
        value_ += String(value).value_;
        return true;
    }

    String &operator+=(const String &rhs)
    {
        concat(rhs);
        return *this;
    }
    String &operator+=(const char *rhs)
    {
        concat(rhs);
        return *this;
    }
    String &operator+=(char rhs)
    {
        concat(rhs);
        return *this;
    }
    String &operator+=(int rhs)
    {
        concat(rhs);
        return *this;
    }
    String &operator+=(unsigned int rhs)
    {
        concat(rhs);
        return *this;
    }
    String &operator+=(long rhs)
    {
        concat(rhs);
        return *this;
    }
    String &operator+=(unsigned long rhs)
    {
        concat(rhs);
        return *this;
    }
    String &operator+=(long long rhs)
    {
        concat(rhs);
        return *this;
    }
    String &operator+=(unsigned long long rhs)
    {
        concat(rhs);
        return *this;
    }
    String &operator+=(float rhs)
    {
        concat(rhs);
        return *this;
    }
    String &operator+=(double rhs)
    {
        concat(rhs);
        return *this;
    }

    int compareTo(const String &other) const { return value_.compare(other.value_); }
    bool equals(const String &other) const { return value_ == other.value_; }
    bool equals(const char *other) const { return value_ == (other ? other : ""); }
    bool equalsIgnoreCase(const String &other) const
    {
        return lower(value_) == lower(other.value_);
    }

    bool startsWith(const String &prefix) const
    {
        return value_.size() >= prefix.value_.size() &&
            value_.compare(0, prefix.value_.size(), prefix.value_) == 0;
    }
    bool startsWith(const String &prefix, unsigned int offset) const
    {
        return offset <= value_.size() &&
            value_.size() - offset >= prefix.value_.size() &&
            value_.compare(offset, prefix.value_.size(), prefix.value_) == 0;
    }
    bool endsWith(const String &suffix) const
    {
        return value_.size() >= suffix.value_.size() &&
            value_.compare(value_.size() - suffix.value_.size(), suffix.value_.size(), suffix.value_) == 0;
    }

    int indexOf(char ch) const { return indexOf(ch, 0); }
    int indexOf(char ch, unsigned int fromIndex) const
    {
        const size_t pos = value_.find(ch, fromIndex);
        return pos == std::string::npos ? -1 : static_cast<int>(pos);
    }
    int indexOf(const String &str) const { return indexOf(str, 0); }
    int indexOf(const String &str, unsigned int fromIndex) const
    {
        const size_t pos = value_.find(str.value_, fromIndex);
        return pos == std::string::npos ? -1 : static_cast<int>(pos);
    }
    int lastIndexOf(char ch) const
    {
        const size_t pos = value_.rfind(ch);
        return pos == std::string::npos ? -1 : static_cast<int>(pos);
    }
    int lastIndexOf(const String &str) const
    {
        const size_t pos = value_.rfind(str.value_);
        return pos == std::string::npos ? -1 : static_cast<int>(pos);
    }

    String substring(unsigned int beginIndex) const { return substring(beginIndex, length()); }
    String substring(unsigned int beginIndex, unsigned int endIndex) const
    {
        if (beginIndex > endIndex) {
            std::swap(beginIndex, endIndex);
        }
        if (beginIndex >= value_.size()) {
            return String();
        }
        return String(value_.substr(beginIndex, endIndex - beginIndex));
    }

    void replace(char find, char replace)
    {
        std::replace(value_.begin(), value_.end(), find, replace);
    }
    void replace(const String &find, const String &replace)
    {
        if (find.value_.empty()) {
            return;
        }
        size_t pos = 0;
        while ((pos = value_.find(find.value_, pos)) != std::string::npos) {
            value_.replace(pos, find.value_.size(), replace.value_);
            pos += replace.value_.size();
        }
    }
    void remove(unsigned int index)
    {
        if (index < value_.size()) {
            value_.erase(index);
        }
    }
    void remove(unsigned int index, unsigned int count)
    {
        if (index < value_.size()) {
            value_.erase(index, count);
        }
    }

    void toLowerCase()
    {
        std::transform(value_.begin(), value_.end(), value_.begin(), toLower);
    }
    void toUpperCase()
    {
        std::transform(value_.begin(), value_.end(), value_.begin(), toUpper);
    }
    void trim()
    {
        size_t first = 0;
        while (first < value_.size() && std::isspace(static_cast<unsigned char>(value_[first]))) {
            ++first;
        }
        size_t last = value_.size();
        while (last > first && std::isspace(static_cast<unsigned char>(value_[last - 1]))) {
            --last;
        }
        value_ = value_.substr(first, last - first);
    }

    long toInt() const { return std::strtol(value_.c_str(), NULL, 10); }
    float toFloat() const { return static_cast<float>(std::strtod(value_.c_str(), NULL)); }
    double toDouble() const { return std::strtod(value_.c_str(), NULL); }

    void getBytes(unsigned char *buf, unsigned int bufsize, unsigned int index = 0) const
    {
        if (!buf || bufsize == 0) {
            return;
        }
        const std::string slice = index < value_.size() ? value_.substr(index, bufsize - 1) : std::string();
        std::memcpy(buf, slice.data(), slice.size());
        buf[slice.size()] = 0;
    }
    void toCharArray(char *buf, unsigned int bufsize, unsigned int index = 0) const
    {
        getBytes(reinterpret_cast<unsigned char *>(buf), bufsize, index);
    }

    operator const char *() const { return value_.c_str(); }
    const std::string &std_str() const { return value_; }

private:
    std::string value_;

    static char toLower(char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
    static char toUpper(char c) { return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); }
    static std::string lower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), toLower);
        return value;
    }
    static std::string formatUnsigned(unsigned long long value, unsigned char base)
    {
        if (base == 10 || base < 2 || base > 36) {
            std::ostringstream oss;
            oss << value;
            return oss.str();
        }
        const char *digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        std::string out;
        do {
            out.insert(out.begin(), digits[value % base]);
            value /= base;
        } while (value);
        return out;
    }
    static std::string formatSigned(long long value, unsigned char base)
    {
        if (base == 10) {
            std::ostringstream oss;
            oss << value;
            return oss.str();
        }
        return formatUnsigned(static_cast<unsigned long long>(value), base);
    }
    static std::string formatFloat(double value, unsigned char decimalPlaces)
    {
        std::ostringstream oss;
        oss.setf(std::ios::fixed);
        oss.precision(decimalPlaces);
        oss << value;
        return oss.str();
    }
};

inline String operator+(const String &lhs, const String &rhs)
{
    String out(lhs);
    out += rhs;
    return out;
}
inline String operator+(const String &lhs, const char *rhs) { return lhs + String(rhs); }
inline String operator+(const char *lhs, const String &rhs) { return String(lhs) + rhs; }
inline String operator+(const String &lhs, char rhs) { return lhs + String(rhs); }
inline String operator+(const String &lhs, int rhs) { return lhs + String(rhs); }
inline String operator+(const String &lhs, unsigned int rhs) { return lhs + String(rhs); }
inline String operator+(const String &lhs, long rhs) { return lhs + String(rhs); }
inline String operator+(const String &lhs, unsigned long rhs) { return lhs + String(rhs); }
inline String operator+(const String &lhs, long long rhs) { return lhs + String(rhs); }
inline String operator+(const String &lhs, unsigned long long rhs) { return lhs + String(rhs); }
inline String operator+(const String &lhs, float rhs) { return lhs + String(rhs); }
inline String operator+(const String &lhs, double rhs) { return lhs + String(rhs); }
inline bool operator==(const String &lhs, const String &rhs) { return lhs.equals(rhs); }
inline bool operator==(const String &lhs, const char *rhs) { return lhs.equals(rhs); }
inline bool operator==(const char *lhs, const String &rhs) { return rhs.equals(lhs); }
inline bool operator!=(const String &lhs, const String &rhs) { return !(lhs == rhs); }
inline bool operator!=(const String &lhs, const char *rhs) { return !(lhs == rhs); }
inline bool operator!=(const char *lhs, const String &rhs) { return !(lhs == rhs); }
inline bool operator<(const String &lhs, const String &rhs) { return lhs.compareTo(rhs) < 0; }
inline bool operator>(const String &lhs, const String &rhs) { return lhs.compareTo(rhs) > 0; }
inline bool operator<=(const String &lhs, const String &rhs) { return lhs.compareTo(rhs) <= 0; }
inline bool operator>=(const String &lhs, const String &rhs) { return lhs.compareTo(rhs) >= 0; }

#endif
