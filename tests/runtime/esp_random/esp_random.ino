// Exercises esp_random / esp_fill_random.

#include <Arduino.h>
#include "esp_random.h"

void setup() {
    Serial.begin(115200);
    Serial.println("TEST start esp_random");

    // Two calls should not always return the same value. Distribution
    // checks are heuristic but extremely unlikely to fail with a real PRNG.
    uint32_t prev = esp_random();
    int distinct = 0;
    for (int i = 0; i < 64; ++i) {
        const uint32_t v = esp_random();
        if (v != prev) ++distinct;
        prev = v;
    }
    Serial.print("distinct=");
    Serial.println(distinct >= 60 ? "ok" : "fail");

    // Fill buffer, check it's not all zeros (overwhelmingly unlikely for 32 bytes).
    uint8_t buf[32] = {0};
    esp_fill_random(buf, sizeof(buf));
    int nonzero = 0;
    for (size_t i = 0; i < sizeof(buf); ++i) if (buf[i] != 0) ++nonzero;
    Serial.print("fill_nonzero=");
    Serial.println(nonzero >= 24 ? "ok" : "fail");

    // Odd-length fill works (path covers the tail loop).
    uint8_t small[5] = {0, 0, 0, 0, 0};
    esp_fill_random(small, sizeof(small));
    int small_nonzero = 0;
    for (size_t i = 0; i < sizeof(small); ++i) if (small[i] != 0) ++small_nonzero;
    Serial.print("small_nonzero=");
    Serial.println(small_nonzero >= 3 ? "ok" : "fail");

    Serial.println("TEST done");
}

void loop() { delay(100); }
