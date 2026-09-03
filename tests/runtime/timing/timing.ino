// Tests for millis() / micros() / delay() / delayMicroseconds().
//
// Verifies monotonicity and that delay(N) actually waits at least N ms
// (with a generous upper bound to avoid flakiness on loaded hosts).
//
// Every check prints its own name on failure, and the summary repeats the
// names, because these are the assertions most likely to fail only on a
// particular CI runner — a count alone leaves nobody able to tell an
// over-tight upper bound from a `delay` that returned early.
//
// `stopping` is reported for the same reason: `delay()` returns
// immediately once the runtime is shutting down, so a dropped TCP
// connection turns every lower bound here into a failure that has nothing
// to do with timing.

#include <Arduino.h>

static int g_pass = 0;
static int g_total = 0;
static String g_failed;

#define EXPECT_TRUE(name, cond)      \
    do                               \
    {                                \
        ++g_total;                   \
        if (cond)                    \
        {                            \
            ++g_pass;                \
            Serial.print("PASS ");   \
            Serial.println(name);    \
        }                            \
        else                         \
        {                            \
            if (g_failed.length())   \
                g_failed += ",";     \
            g_failed += name;        \
            Serial.print("FAIL ");   \
            Serial.println(name);    \
        }                            \
    } while (0)

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start timing");

    const uint32_t t0_ms = millis();
    const uint32_t t0_us = micros();

    // The clock counts from the first reading a process makes, so that
    // reading is near zero. This is here as its own named check because
    // getting it wrong poisons every elapsed-time figure below: a bad
    // first `millis()` made `delay50_upper` fail too, on macOS only,
    // when `clockRealNowMicros` latched its epoch after the reading
    // instead of before it.
    EXPECT_TRUE("first_reading_sane", t0_ms < 1000 && t0_us < 1000000UL);

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

    // Reported before the summary so a failure message can name both the
    // checks that failed and whether the runtime was already shutting
    // down, which would explain every lower bound at once.
    Serial.print("stopping=");
    Serial.println(HostArduino::runtimeShouldStop() ? 1 : 0);
    Serial.print("failed=");
    Serial.println(g_failed.length() ? g_failed : String("none"));

    Serial.print("TEST done ");
    Serial.print(g_pass);
    Serial.print("/");
    Serial.println(g_total);
}

void loop()
{
    delay(10);
}
