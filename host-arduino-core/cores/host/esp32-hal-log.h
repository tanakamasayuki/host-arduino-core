#ifndef HOST_ARDUINO_ESP32_HAL_LOG_H
#define HOST_ARDUINO_ESP32_HAL_LOG_H

// Host stub for esp32-hal-log.h / esp_log.h.
//
// CORE_DEBUG_LEVEL is fixed at 0 (NONE) on the host — log macros expand to a
// statement that references all arguments via (void)sizeof so unused-variable
// and unused-parameter warnings don't fire at call sites, but nothing is
// emitted at runtime. This is intentional: ESP_LOG output would mix with
// pytest's `dut.expect` stream, and Arduino's own convention is to use
// Serial.print for diagnostic output. There is no opt-in switch.

#ifndef ARDUHAL_LOG_LEVEL_NONE
#define ARDUHAL_LOG_LEVEL_NONE    0
#define ARDUHAL_LOG_LEVEL_ERROR   1
#define ARDUHAL_LOG_LEVEL_WARN    2
#define ARDUHAL_LOG_LEVEL_INFO    3
#define ARDUHAL_LOG_LEVEL_DEBUG   4
#define ARDUHAL_LOG_LEVEL_VERBOSE 5
#endif

#ifndef CORE_DEBUG_LEVEL
#define CORE_DEBUG_LEVEL ARDUHAL_LOG_LEVEL_NONE
#endif

#ifndef ESP_LOG_NONE
#define ESP_LOG_NONE    0
#define ESP_LOG_ERROR   1
#define ESP_LOG_WARN    2
#define ESP_LOG_INFO    3
#define ESP_LOG_DEBUG   4
#define ESP_LOG_VERBOSE 5
#endif

typedef int esp_log_level_t;

// Discard all arguments without evaluating them, but reference each via
// sizeof so the compiler still type-checks and "unused variable" warnings
// stay quiet at the call site.
#define HOST_ARDUINO_LOG_DISCARD(...) \
    do { (void)sizeof(#__VA_ARGS__); } while (0)

#define log_v(...) HOST_ARDUINO_LOG_DISCARD(__VA_ARGS__)
#define log_d(...) HOST_ARDUINO_LOG_DISCARD(__VA_ARGS__)
#define log_i(...) HOST_ARDUINO_LOG_DISCARD(__VA_ARGS__)
#define log_w(...) HOST_ARDUINO_LOG_DISCARD(__VA_ARGS__)
#define log_e(...) HOST_ARDUINO_LOG_DISCARD(__VA_ARGS__)
#define log_n(...) HOST_ARDUINO_LOG_DISCARD(__VA_ARGS__)

#define isr_log_v(...) HOST_ARDUINO_LOG_DISCARD(__VA_ARGS__)
#define isr_log_d(...) HOST_ARDUINO_LOG_DISCARD(__VA_ARGS__)
#define isr_log_i(...) HOST_ARDUINO_LOG_DISCARD(__VA_ARGS__)
#define isr_log_w(...) HOST_ARDUINO_LOG_DISCARD(__VA_ARGS__)
#define isr_log_e(...) HOST_ARDUINO_LOG_DISCARD(__VA_ARGS__)
#define isr_log_n(...) HOST_ARDUINO_LOG_DISCARD(__VA_ARGS__)

#define ESP_LOGE(tag, ...) HOST_ARDUINO_LOG_DISCARD(tag, __VA_ARGS__)
#define ESP_LOGW(tag, ...) HOST_ARDUINO_LOG_DISCARD(tag, __VA_ARGS__)
#define ESP_LOGI(tag, ...) HOST_ARDUINO_LOG_DISCARD(tag, __VA_ARGS__)
#define ESP_LOGD(tag, ...) HOST_ARDUINO_LOG_DISCARD(tag, __VA_ARGS__)
#define ESP_LOGV(tag, ...) HOST_ARDUINO_LOG_DISCARD(tag, __VA_ARGS__)

#define ESP_EARLY_LOGE(tag, ...) HOST_ARDUINO_LOG_DISCARD(tag, __VA_ARGS__)
#define ESP_EARLY_LOGW(tag, ...) HOST_ARDUINO_LOG_DISCARD(tag, __VA_ARGS__)
#define ESP_EARLY_LOGI(tag, ...) HOST_ARDUINO_LOG_DISCARD(tag, __VA_ARGS__)
#define ESP_EARLY_LOGD(tag, ...) HOST_ARDUINO_LOG_DISCARD(tag, __VA_ARGS__)
#define ESP_EARLY_LOGV(tag, ...) HOST_ARDUINO_LOG_DISCARD(tag, __VA_ARGS__)

#define ESP_DRAM_LOGE(tag, ...) HOST_ARDUINO_LOG_DISCARD(tag, __VA_ARGS__)
#define ESP_DRAM_LOGW(tag, ...) HOST_ARDUINO_LOG_DISCARD(tag, __VA_ARGS__)
#define ESP_DRAM_LOGI(tag, ...) HOST_ARDUINO_LOG_DISCARD(tag, __VA_ARGS__)
#define ESP_DRAM_LOGD(tag, ...) HOST_ARDUINO_LOG_DISCARD(tag, __VA_ARGS__)
#define ESP_DRAM_LOGV(tag, ...) HOST_ARDUINO_LOG_DISCARD(tag, __VA_ARGS__)

inline void esp_log_level_set(const char * /*tag*/, esp_log_level_t /*level*/) {}
inline esp_log_level_t esp_log_level_get(const char * /*tag*/) { return ESP_LOG_NONE; }
inline void esp_log_write(esp_log_level_t /*level*/, const char * /*tag*/, const char * /*format*/, ...) {}
inline uint32_t esp_log_timestamp(void) { return 0; }

#endif // HOST_ARDUINO_ESP32_HAL_LOG_H
