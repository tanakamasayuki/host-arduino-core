// Exercises xQueueCreate / xQueueSend / xQueueReceive across a producer task.

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static QueueHandle_t g_queue = nullptr;

static void producer(void *arg) {
    (void)arg;
    for (uint32_t i = 1; i <= 5; ++i) {
        xQueueSend(g_queue, &i, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    vTaskDelete(NULL);
}

void setup() {
    Serial.begin(115200);
    Serial.println("TEST start freertos_queue");

    g_queue = xQueueCreate(4, sizeof(uint32_t));
    Serial.print("create=");
    Serial.println(g_queue != nullptr ? "ok" : "fail");

    xTaskCreate(producer, "producer", 4096, nullptr, 1, nullptr);

    uint32_t sum = 0;
    uint32_t count = 0;
    while (count < 5) {
        uint32_t v = 0;
        if (xQueueReceive(g_queue, &v, pdMS_TO_TICKS(1000)) == pdPASS) {
            sum += v;
            ++count;
        } else {
            break;
        }
    }

    Serial.print("count=");
    Serial.println(count);
    Serial.print("sum=");
    Serial.println(sum);

    // Empty-queue timeout
    uint32_t dummy = 0;
    const BaseType_t rc = xQueueReceive(g_queue, &dummy, pdMS_TO_TICKS(50));
    Serial.print("timeout=");
    Serial.println(rc == pdFAIL ? "ok" : "fail");

    vQueueDelete(g_queue);
    Serial.println("TEST done");
}

void loop() { delay(100); }
