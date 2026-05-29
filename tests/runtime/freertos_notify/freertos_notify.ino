// Exercises xTaskNotifyGive / ulTaskNotifyTake.

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <atomic>

static TaskHandle_t g_consumer = nullptr;
static std::atomic<uint32_t> g_received{0};
static std::atomic<bool> g_release{false};
static std::atomic<bool> g_done{false};

static void consumer(void *arg) {
    (void)arg;
    while (!g_release.load()) {
        vTaskDelay(1);
    }
    const uint32_t v = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2000));
    g_received.store(v);
    g_done.store(true);
    vTaskDelete(NULL);
}

void setup() {
    Serial.begin(115200);
    Serial.println("TEST start freertos_notify");

    xTaskCreate(consumer, "consumer", 4096, nullptr, 1, &g_consumer);

    xTaskNotifyGive(g_consumer);
    xTaskNotifyGive(g_consumer);
    xTaskNotifyGive(g_consumer);
    g_release.store(true);

    const uint32_t t0 = millis();
    while (!g_done.load() && (millis() - t0) < 2000) {
        delay(10);
    }

    Serial.print("received=");
    Serial.println(g_received.load());
    Serial.print("done=");
    Serial.println(g_done.load() ? "1" : "0");

    Serial.println("TEST done");
}

void loop() { delay(100); }
