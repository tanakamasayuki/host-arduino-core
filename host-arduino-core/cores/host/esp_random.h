#ifndef HOST_ARDUINO_ESP_RANDOM_H
#define HOST_ARDUINO_ESP_RANDOM_H

// Host stub for ESP-IDF esp_random / esp_fill_random. Backed by
// std::random_device + std::mt19937 — adequate for non-cryptographic uses
// (which is also what the silicon's hardware RNG is typically used for in
// Arduino sketches: jitter, seeding, simple variety).

#include <stdint.h>
#include <stddef.h>

#include <mutex>
#include <random>

namespace host_esp_random {

inline std::mt19937 &engine() {
    static std::mt19937 e{std::random_device{}()};
    return e;
}
inline std::mutex &lock() {
    static std::mutex m;
    return m;
}

} // namespace host_esp_random

inline uint32_t esp_random(void) {
    std::lock_guard<std::mutex> g(host_esp_random::lock());
    return static_cast<uint32_t>(host_esp_random::engine()());
}

inline void esp_fill_random(void *buf, size_t len) {
    if (!buf) return;
    uint8_t *p = static_cast<uint8_t *>(buf);
    std::lock_guard<std::mutex> g(host_esp_random::lock());
    auto &e = host_esp_random::engine();
    size_t i = 0;
    while (i + 4 <= len) {
        const uint32_t v = static_cast<uint32_t>(e());
        p[i + 0] = static_cast<uint8_t>(v);
        p[i + 1] = static_cast<uint8_t>(v >> 8);
        p[i + 2] = static_cast<uint8_t>(v >> 16);
        p[i + 3] = static_cast<uint8_t>(v >> 24);
        i += 4;
    }
    if (i < len) {
        uint32_t v = static_cast<uint32_t>(e());
        while (i < len) {
            p[i++] = static_cast<uint8_t>(v);
            v >>= 8;
        }
    }
}

#endif // HOST_ARDUINO_ESP_RANDOM_H
