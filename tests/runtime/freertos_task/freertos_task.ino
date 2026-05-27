// Exercises xTaskCreate / vTaskDelay / vTaskDelete on the host runtime.

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <atomic>

static std::atomic<int> g_counter{0};
static std::atomic<bool> g_done{false};

static void worker(void *arg) {
    const int target = *static_cast<int *>(arg);
    for (int i = 0; i < target; ++i) {
        g_counter.fetch_add(1);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    g_done.store(true);
    vTaskDelete(NULL);
}

void setup() {
    Serial.begin(115200);
    Serial.println("TEST start freertos_task");

    static int target = 5;
    TaskHandle_t h = nullptr;
    const BaseType_t rc = xTaskCreate(worker, "worker", 4096, &target, 1, &h);
    Serial.print("create=");
    Serial.println(rc == pdPASS ? "ok" : "fail");
    Serial.print("handle=");
    Serial.println(h != nullptr ? "ok" : "fail");

    const uint32_t t0 = millis();
    while (!g_done.load() && (millis() - t0) < 2000) {
        delay(10);
    }

    Serial.print("counter=");
    Serial.println(g_counter.load());
    Serial.print("done=");
    Serial.println(g_done.load() ? "1" : "0");

    Serial.println("TEST done");
}

void loop() { delay(100); }
