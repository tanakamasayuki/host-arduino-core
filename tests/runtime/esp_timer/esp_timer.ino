// Exercises esp_timer_get_time + esp_timer_create + start_periodic + stop.

#include <Arduino.h>
#include "esp_timer.h"

#include <atomic>

static std::atomic<int> g_ticks{0};

static void tick_cb(void *arg) {
    (void)arg;
    g_ticks.fetch_add(1);
}

void setup() {
    Serial.begin(115200);
    Serial.println("TEST start esp_timer");

    const int64_t t0 = esp_timer_get_time();
    delay(50);
    const int64_t t1 = esp_timer_get_time();
    const int64_t elapsed_us = t1 - t0;
    Serial.print("monotonic=");
    Serial.println(elapsed_us > 0 ? "ok" : "fail");
    Serial.print("delay_lower=");
    Serial.println(elapsed_us >= 45000 ? "ok" : "fail");
    Serial.print("delay_upper=");
    Serial.println(elapsed_us < 500000 ? "ok" : "fail");

    esp_timer_create_args_t args = {};
    args.callback = tick_cb;
    args.arg = nullptr;
    args.name = "tick";

    esp_timer_handle_t h = nullptr;
    const esp_err_t rc = esp_timer_create(&args, &h);
    Serial.print("create=");
    Serial.println(rc == ESP_OK ? "ok" : "fail");

    esp_timer_start_periodic(h, 20000); // 20ms
    delay(500);
    esp_timer_stop(h);
    const int fired = g_ticks.load();
    Serial.print("fired_lower=");
    Serial.println(fired >= 4 ? "ok" : "fail");
    Serial.print("fired_upper=");
    Serial.println(fired <= 50 ? "ok" : "fail");

    // After stop, no more ticks.
    delay(80);
    const int after_stop = g_ticks.load() - fired;
    Serial.print("after_stop=");
    Serial.println(after_stop <= 1 ? "ok" : "fail");

    esp_timer_delete(h);
    Serial.println("TEST done");
}

void loop() { delay(100); }
