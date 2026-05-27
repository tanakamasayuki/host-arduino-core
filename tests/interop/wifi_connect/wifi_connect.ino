// interop/wifi_connect — confirms WiFi.begin / status / localIP work
// with the same source on both host and ESP32 profiles.
//
// Expected output on either target:
//   WIFI_OK <ipv4>
// Or on failure:
//   WIFI_ERROR <reason>
//
// On the host runtime the WiFi facade is a state-tracked stub:
// `begin()` immediately reports `WL_CONNECTED` and `localIP()` returns
// 127.0.0.1, so the polling loop exits on its first iteration. On real
// ESP32 silicon `begin()` triggers the actual association and the loop
// polls until DHCP completes or the 60-second timeout expires.

#include <Arduino.h>
#include <WiFi.h>

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

constexpr unsigned long WIFI_TIMEOUT_MS = 60000;

void setup()
{
    Serial.begin(115200);
    delay(5000);

    if (String(WIFI_SSID).isEmpty())
    {
        Serial.println("WIFI_ERROR missing_ssid");
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    const unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS)
    {
        delay(250);
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.print("WIFI_ERROR connect_failed status=");
        Serial.println((int)WiFi.status());
        return;
    }

    Serial.print("WIFI_OK ");
    Serial.println(WiFi.localIP());
}

void loop()
{
    delay(1000);
}
