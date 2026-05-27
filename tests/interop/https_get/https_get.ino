// interop/https_get — HTTPS GET via HTTPClient against httpbin.org,
// proving that TLS handshake + HTTP request + response body retrieval
// behave the same on the host (OpenSSL backend, cert verification
// skipped) and on real ESP32 silicon (mbedTLS backend).
//
// The sketch sends a unique header value and verifies it later by
// looking for the same value in the JSON body that httpbin.org echoes
// back. This proves the full request/response round-trip end-to-end
// without depending on the JSON schema staying byte-stable.
//
// Host profile requires the `tls=openssl` board menu option (set in
// this directory's sketch.yaml).

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

constexpr unsigned long WIFI_TIMEOUT_MS = 60000;

static const char *TEST_URL = "https://httpbin.org/get";
static const char *TEST_HEADER_NAME = "X-Test-Tag";
static const char *TEST_HEADER_VALUE = "host-arduino-core-interop";

HTTPClient http;

static bool connect_wifi()
{
    if (String(WIFI_SSID).isEmpty())
    {
        Serial.println("WIFI_ERROR missing_ssid");
        return false;
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
        return false;
    }
    Serial.print("WIFI_OK ");
    Serial.println(WiFi.localIP());
    return true;
}

void setup()
{
    Serial.begin(115200);
    delay(5000);

    if (!connect_wifi())
        return;

    if (!http.begin(TEST_URL))
    {
        Serial.println("HTTP_BEGIN_FAIL");
        return;
    }
    http.setTimeout(15000);
    http.addHeader(TEST_HEADER_NAME, TEST_HEADER_VALUE);

    const int code = http.GET();
    Serial.print("CODE=");
    Serial.println(code);

    if (code > 0)
    {
        const String body = http.getString();
        Serial.print("BODY_LEN=");
        Serial.println((int)body.length());
        Serial.println("BODY_BEGIN");
        Serial.println(body);
        Serial.println("BODY_END");
    }
    http.end();
    Serial.println("DONE");
}

void loop()
{
    delay(1000);
}
