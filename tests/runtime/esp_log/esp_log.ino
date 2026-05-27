// Verifies the host log stubs: macros compile, expand to nothing observable,
// and CORE_DEBUG_LEVEL is fixed at NONE.

#include <Arduino.h>
#include "esp_log.h"

void setup() {
    Serial.begin(115200);
    Serial.println("TEST start esp_log");

    Serial.print("CORE_DEBUG_LEVEL=");
    Serial.println(CORE_DEBUG_LEVEL);
    Serial.print("ESP_LOG_NONE=");
    Serial.println(ESP_LOG_NONE);
    Serial.print("ESP_LOG_VERBOSE=");
    Serial.println(ESP_LOG_VERBOSE);

    Serial.println("BEFORE");

    // These must compile and emit nothing.
    int unused_value = 42;
    const char *unused_str = "hello";
    log_e("err %d", unused_value);
    log_w("warn %d", unused_value);
    log_i("info %s", unused_str);
    log_d("debug %s", unused_str);
    log_v("verbose %d %s", unused_value, unused_str);

    ESP_LOGE("TAG", "err %d", unused_value);
    ESP_LOGW("TAG", "warn %d", unused_value);
    ESP_LOGI("TAG", "info %s", unused_str);
    ESP_LOGD("TAG", "debug %s", unused_str);
    ESP_LOGV("TAG", "verbose %d %s", unused_value, unused_str);

    esp_log_level_set("TAG", ESP_LOG_DEBUG);
    Serial.print("level_get=");
    Serial.println(static_cast<int>(esp_log_level_get("TAG")));

    Serial.println("AFTER");
    Serial.println("TEST done");
}

void loop() { delay(100); }
