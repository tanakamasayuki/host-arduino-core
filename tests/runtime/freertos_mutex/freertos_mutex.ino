// Exercises xSemaphoreCreateMutex / Take / Give and binary/counting semaphores.
//
// What is being verified is that the mutex loses no updates: 3 tasks x 1000
// increments must come out as exactly 3000. How long that takes is not part
// of the claim, so the wait reports whether it ran out — a slow runner
// should never be mistaken for a lost update, which is exactly what
// happened once: a bad first `millis()` reading made `millis() - t0` wrap,
// the wait loop never ran at all, and `shared` came out at whatever the
// tasks had managed by then.

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
    const int done = g_done_workers.load();
    Serial.print("workers_done=");
    Serial.println(done);
    Serial.print("wait_timeout=");
    Serial.println(done < 3 ? 1 : 0);
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
