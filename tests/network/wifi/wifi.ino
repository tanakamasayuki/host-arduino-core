// Tests for the WiFi stub: status / localIP / SSID transitions
// across begin() / disconnect() / mode(WIFI_OFF).

#include <Arduino.h>
#include <WiFi.h>

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
    Serial.println("TEST start wifi");

    EXPECT_TRUE("initial_idle", WiFi.status() == WL_IDLE_STATUS);
    EXPECT_TRUE("initial_ip_zero", WiFi.localIP() == IPAddress(0, 0, 0, 0));
    EXPECT_TRUE("initial_ssid_empty", String(WiFi.SSID()) == "");

    WiFi.begin("test-ssid", "test-pass");
    EXPECT_TRUE("after_begin_connected", WiFi.status() == WL_CONNECTED);
    EXPECT_TRUE("after_begin_ip", WiFi.localIP() == IPAddress(127, 0, 0, 1));
    EXPECT_TRUE("after_begin_ssid", String(WiFi.SSID()) == "test-ssid");

    WiFi.disconnect();
    EXPECT_TRUE("after_disconnect", WiFi.status() == WL_DISCONNECTED);
    EXPECT_TRUE("after_disconnect_ip", WiFi.localIP() == IPAddress(0, 0, 0, 0));
    EXPECT_TRUE("after_disconnect_ssid", String(WiFi.SSID()) == "");

    WiFi.begin("again");
    EXPECT_TRUE("rebegin_connected", WiFi.status() == WL_CONNECTED);

    WiFi.mode(WIFI_OFF);
    EXPECT_TRUE("mode_off_idle", WiFi.status() == WL_IDLE_STATUS);
    EXPECT_TRUE("mode_off_ip", WiFi.localIP() == IPAddress(0, 0, 0, 0));

    Serial.print("TEST done ");
    Serial.print(g_pass);
    Serial.print("/");
    Serial.println(g_total);
}

void loop()
{
    delay(10);
}
