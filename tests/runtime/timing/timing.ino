// Tests for millis() / micros() / delay() / delayMicroseconds().
//
// Verifies monotonicity and that delay(N) actually waits at least N ms
// (with a generous upper bound to avoid flakiness on loaded hosts).

#include <Arduino.h>

static int g_pass = 0;
static int g_total = 0;

#define EXPECT_TRUE(name, cond)    \
    do                             \
    {                              \
        ++g_total;                 \
        if (cond)                  \
        {                          \
            ++g_pass;              \
            Serial.print("PASS "); \
            Serial.println(name);  \
        }                          \
        else                       \
        {                          \
            Serial.print("FAIL "); \
            Serial.println(name);  \
        }                          \
    } while (0)

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start timing");

    const uint32_t t0_ms = millis();
    const uint32_t t0_us = micros();

    EXPECT_TRUE("micros>=millis*1000-1", t0_us + 1000 >= t0_ms * 1000UL);

    delay(50);
    const uint32_t t1_ms = millis();
    const uint32_t elapsed_ms = t1_ms - t0_ms;
    EXPECT_TRUE("delay50_lower", elapsed_ms >= 45);
    EXPECT_TRUE("delay50_upper", elapsed_ms < 500);

    delayMicroseconds(2000);
    const uint32_t t2_us = micros();
    const uint32_t elapsed_us = t2_us - t0_us;
    EXPECT_TRUE("delayUs_lower", elapsed_us >= 50000UL); // 50ms + 2ms total
    EXPECT_TRUE("delayUs_upper", elapsed_us < 1000000UL);

    EXPECT_TRUE("monotonic_ms", millis() >= t1_ms);
    EXPECT_TRUE("monotonic_us", micros() >= t2_us);

    Serial.print("TEST done ");
    Serial.print(g_pass);
    Serial.print("/");
    Serial.println(g_total);
}

void loop()
{
    delay(10);
}
