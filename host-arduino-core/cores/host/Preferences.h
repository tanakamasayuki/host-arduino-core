#ifndef HOST_ARDUINO_PREFERENCES_H
#define HOST_ARDUINO_PREFERENCES_H

// In-memory implementation of the ESP32 Preferences (NVS) API.
//
// All data lives in a process-wide std::map keyed by namespace. Nothing is
// persisted to disk — values disappear when the host sketch exits. Reading a
// missing key returns the supplied default value, which matches the real
// API's contract on ESP32.
//
// Boot-crossing behavior cannot be tested on host. Use an interop test
// against a real ESP32 if persistence is what you need to verify.

#include <stdint.h>
#include <stddef.h>

#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "WString.h"

typedef enum {
    PT_I8,
    PT_U8,
    PT_I16,
    PT_U16,
    PT_I32,
    PT_U32,
    PT_I64,
    PT_U64,
    PT_STR,
    PT_BLOB,
    PT_INVALID,
} PreferenceType;

namespace host_preferences {

struct Entry {
    PreferenceType type = PT_INVALID;
    std::vector<uint8_t> bytes; // raw little-endian for scalars; UTF-8 (no NUL) for strings; raw for blobs
};

struct Namespace {
    std::map<std::string, Entry> entries;
};

inline std::mutex &store_mutex() {
    static std::mutex m;
    return m;
}
inline std::map<std::string, Namespace> &store() {
    static std::map<std::string, Namespace> s;
    return s;
}

template <typename T>
inline std::vector<uint8_t> pack_scalar(T value) {
    std::vector<uint8_t> b(sizeof(T));
    std::memcpy(b.data(), &value, sizeof(T));
    return b;
}

template <typename T>
inline bool unpack_scalar(const Entry &e, T &out) {
    if (e.bytes.size() != sizeof(T)) return false;
    std::memcpy(&out, e.bytes.data(), sizeof(T));
    return true;
}

} // namespace host_preferences

class Preferences {
public:
    Preferences() = default;
    ~Preferences() { end(); }

    bool begin(const char *name, bool readOnly = false, const char * /*partition*/ = nullptr) {
        if (!name || !*name) return false;
        std::lock_guard<std::mutex> g(host_preferences::store_mutex());
        ns_ = name;
        read_only_ = readOnly;
        started_ = true;
        host_preferences::store()[ns_];
        return true;
    }

    void end() {
        started_ = false;
        ns_.clear();
    }

    bool clear() {
        if (!started_ || read_only_) return false;
        std::lock_guard<std::mutex> g(host_preferences::store_mutex());
        host_preferences::store()[ns_].entries.clear();
        return true;
    }

    bool remove(const char *key) {
        if (!started_ || read_only_ || !key) return false;
        std::lock_guard<std::mutex> g(host_preferences::store_mutex());
        return host_preferences::store()[ns_].entries.erase(key) > 0;
    }

    bool isKey(const char *key) {
        if (!started_ || !key) return false;
        std::lock_guard<std::mutex> g(host_preferences::store_mutex());
        const auto &m = host_preferences::store()[ns_].entries;
        return m.find(key) != m.end();
    }

    PreferenceType getType(const char *key) {
        if (!started_ || !key) return PT_INVALID;
        std::lock_guard<std::mutex> g(host_preferences::store_mutex());
        const auto &m = host_preferences::store()[ns_].entries;
        auto it = m.find(key);
        return it == m.end() ? PT_INVALID : it->second.type;
    }

    size_t freeEntries() { return 1000000; }

    // ---- put scalars ----
    size_t putChar(const char *k, int8_t v)     { return putScalar(k, PT_I8,  v); }
    size_t putUChar(const char *k, uint8_t v)   { return putScalar(k, PT_U8,  v); }
    size_t putShort(const char *k, int16_t v)   { return putScalar(k, PT_I16, v); }
    size_t putUShort(const char *k, uint16_t v) { return putScalar(k, PT_U16, v); }
    size_t putInt(const char *k, int32_t v)     { return putScalar(k, PT_I32, v); }
    size_t putUInt(const char *k, uint32_t v)   { return putScalar(k, PT_U32, v); }
    size_t putLong(const char *k, int32_t v)    { return putScalar(k, PT_I32, v); }
    size_t putULong(const char *k, uint32_t v)  { return putScalar(k, PT_U32, v); }
    size_t putLong64(const char *k, int64_t v)  { return putScalar(k, PT_I64, v); }
    size_t putULong64(const char *k, uint64_t v){ return putScalar(k, PT_U64, v); }
    size_t putFloat(const char *k, float v)     { return putScalar(k, PT_U32, *reinterpret_cast<uint32_t *>(&v)); }
    size_t putDouble(const char *k, double v)   { return putScalar(k, PT_U64, *reinterpret_cast<uint64_t *>(&v)); }
    size_t putBool(const char *k, bool v)       { return putScalar(k, PT_U8, static_cast<uint8_t>(v ? 1 : 0)); }

    size_t putString(const char *k, const char *v) {
        if (!ready_to_write(k) || !v) return 0;
        std::lock_guard<std::mutex> g(host_preferences::store_mutex());
        auto &e = host_preferences::store()[ns_].entries[k];
        e.type = PT_STR;
        const size_t n = std::strlen(v);
        e.bytes.assign(v, v + n);
        return n;
    }
    size_t putString(const char *k, const String &v) { return putString(k, v.c_str()); }

    size_t putBytes(const char *k, const void *buf, size_t len) {
        if (!ready_to_write(k) || (!buf && len > 0)) return 0;
        std::lock_guard<std::mutex> g(host_preferences::store_mutex());
        auto &e = host_preferences::store()[ns_].entries[k];
        e.type = PT_BLOB;
        const uint8_t *p = static_cast<const uint8_t *>(buf);
        e.bytes.assign(p, p + len);
        return len;
    }

    // ---- get scalars ----
    int8_t   getChar(const char *k, int8_t d = 0)     { return getScalar<int8_t>(k, PT_I8, d); }
    uint8_t  getUChar(const char *k, uint8_t d = 0)   { return getScalar<uint8_t>(k, PT_U8, d); }
    int16_t  getShort(const char *k, int16_t d = 0)   { return getScalar<int16_t>(k, PT_I16, d); }
    uint16_t getUShort(const char *k, uint16_t d = 0) { return getScalar<uint16_t>(k, PT_U16, d); }
    int32_t  getInt(const char *k, int32_t d = 0)     { return getScalar<int32_t>(k, PT_I32, d); }
    uint32_t getUInt(const char *k, uint32_t d = 0)   { return getScalar<uint32_t>(k, PT_U32, d); }
    int32_t  getLong(const char *k, int32_t d = 0)    { return getScalar<int32_t>(k, PT_I32, d); }
    uint32_t getULong(const char *k, uint32_t d = 0)  { return getScalar<uint32_t>(k, PT_U32, d); }
    int64_t  getLong64(const char *k, int64_t d = 0)  { return getScalar<int64_t>(k, PT_I64, d); }
    uint64_t getULong64(const char *k, uint64_t d = 0){ return getScalar<uint64_t>(k, PT_U64, d); }

    float getFloat(const char *k, float d = 0.0f) {
        const uint32_t bits = getScalar<uint32_t>(k, PT_U32, *reinterpret_cast<uint32_t *>(&d));
        float out;
        std::memcpy(&out, &bits, sizeof(out));
        return out;
    }
    double getDouble(const char *k, double d = 0.0) {
        const uint64_t bits = getScalar<uint64_t>(k, PT_U64, *reinterpret_cast<uint64_t *>(&d));
        double out;
        std::memcpy(&out, &bits, sizeof(out));
        return out;
    }
    bool getBool(const char *k, bool d = false) {
        return getScalar<uint8_t>(k, PT_U8, static_cast<uint8_t>(d ? 1 : 0)) != 0;
    }

    String getString(const char *k, const String &d) {
        if (!started_ || !k) return d;
        std::lock_guard<std::mutex> g(host_preferences::store_mutex());
        const auto &m = host_preferences::store()[ns_].entries;
        auto it = m.find(k);
        if (it == m.end() || it->second.type != PT_STR) return d;
        const auto &b = it->second.bytes;
        std::string tmp(reinterpret_cast<const char *>(b.data()), b.size());
        return String(tmp.c_str());
    }
    String getString(const char *k) { return getString(k, String("")); }
    size_t getString(const char *k, char *out, size_t maxLen) {
        if (!started_ || !k || !out || maxLen == 0) return 0;
        std::lock_guard<std::mutex> g(host_preferences::store_mutex());
        const auto &m = host_preferences::store()[ns_].entries;
        auto it = m.find(k);
        if (it == m.end() || it->second.type != PT_STR) { out[0] = '\0'; return 0; }
        const auto &b = it->second.bytes;
        const size_t n = b.size() + 1 <= maxLen ? b.size() : (maxLen - 1);
        std::memcpy(out, b.data(), n);
        out[n] = '\0';
        return n;
    }

    size_t getBytesLength(const char *k) {
        if (!started_ || !k) return 0;
        std::lock_guard<std::mutex> g(host_preferences::store_mutex());
        const auto &m = host_preferences::store()[ns_].entries;
        auto it = m.find(k);
        if (it == m.end() || it->second.type != PT_BLOB) return 0;
        return it->second.bytes.size();
    }
    size_t getBytes(const char *k, void *out, size_t maxLen) {
        if (!started_ || !k || !out) return 0;
        std::lock_guard<std::mutex> g(host_preferences::store_mutex());
        const auto &m = host_preferences::store()[ns_].entries;
        auto it = m.find(k);
        if (it == m.end() || it->second.type != PT_BLOB) return 0;
        const auto &b = it->second.bytes;
        const size_t n = b.size() <= maxLen ? b.size() : maxLen;
        std::memcpy(out, b.data(), n);
        return n;
    }

private:
    bool ready_to_write(const char *k) const { return started_ && !read_only_ && k != nullptr; }

    template <typename T>
    size_t putScalar(const char *k, PreferenceType t, T v) {
        if (!ready_to_write(k)) return 0;
        std::lock_guard<std::mutex> g(host_preferences::store_mutex());
        auto &e = host_preferences::store()[ns_].entries[k];
        e.type = t;
        e.bytes = host_preferences::pack_scalar(v);
        return sizeof(T);
    }
    template <typename T>
    T getScalar(const char *k, PreferenceType expected, T d) {
        if (!started_ || !k) return d;
        std::lock_guard<std::mutex> g(host_preferences::store_mutex());
        const auto &m = host_preferences::store()[ns_].entries;
        auto it = m.find(k);
        if (it == m.end() || it->second.type != expected) return d;
        T out;
        if (!host_preferences::unpack_scalar(it->second, out)) return d;
        return out;
    }

    std::string ns_;
    bool started_ = false;
    bool read_only_ = false;
};

#endif // HOST_ARDUINO_PREFERENCES_H
