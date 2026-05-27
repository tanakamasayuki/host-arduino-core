// Exercises xSemaphoreCreateMutex / Take / Give and binary/counting semaphores.

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include <atomic>

static SemaphoreHandle_t g_mutex = nullptr;
static int g_shared = 0;
static std::atomic<int> g_done_workers{0};

static void incrementer(void *arg) {
    (void)arg;
    for (int i = 0; i < 1000; ++i) {
        xSemaphoreTake(g_mutex, portMAX_DELAY);
        g_shared++;
        xSemaphoreGive(g_mutex);
    }
    g_done_workers.fetch_add(1);
    vTaskDelete(NULL);
}

void setup() {
    Serial.begin(115200);
    Serial.println("TEST start freertos_mutex");

    g_mutex = xSemaphoreCreateMutex();
    Serial.print("mutex_create=");
    Serial.println(g_mutex != nullptr ? "ok" : "fail");

    xTaskCreate(incrementer, "a", 4096, nullptr, 1, nullptr);
    xTaskCreate(incrementer, "b", 4096, nullptr, 1, nullptr);
    xTaskCreate(incrementer, "c", 4096, nullptr, 1, nullptr);

    const uint32_t t0 = millis();
    while (g_done_workers.load() < 3 && (millis() - t0) < 5000) {
        delay(10);
    }
    Serial.print("shared=");
    Serial.println(g_shared);

    // Binary semaphore: starts empty.
    SemaphoreHandle_t bin = xSemaphoreCreateBinary();
    const BaseType_t empty_take = xSemaphoreTake(bin, 0);
    Serial.print("bin_empty=");
    Serial.println(empty_take == pdFAIL ? "ok" : "fail");
    xSemaphoreGive(bin);
    const BaseType_t after_give = xSemaphoreTake(bin, 0);
    Serial.print("bin_after_give=");
    Serial.println(after_give == pdPASS ? "ok" : "fail");
    vSemaphoreDelete(bin);

    // Counting semaphore: 3 slots.
    SemaphoreHandle_t cnt = xSemaphoreCreateCounting(3, 3);
    int taken = 0;
    while (xSemaphoreTake(cnt, 0) == pdPASS) ++taken;
    Serial.print("cnt_taken=");
    Serial.println(taken);
    vSemaphoreDelete(cnt);

    vSemaphoreDelete(g_mutex);
    Serial.println("TEST done");
}

void loop() { delay(100); }
