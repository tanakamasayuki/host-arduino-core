// interop/http_chunked — HTTPS + Transfer-Encoding: chunked parity.
//
// httpbin.org/stream/N streams N JSON objects, one per chunk, with
// Transfer-Encoding: chunked and no Content-Length. Each echoed
// object contains the request headers (including our X-Test-Tag), so
// the tag should appear N times in the decoded body if chunked decoding
// works on both host (OpenSSL) and ESP32 (mbedTLS) targets.

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
constexpr int STREAM_COUNT = 3;

static const char *TEST_URL = "https://httpbin.org/stream/3";
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
    Serial.print("LEN=");
    Serial.println(http.getSize()); // -1 expected for chunked

    if (code > 0)
    {
        const String body = http.getString();
        Serial.print("BODY_LEN=");
        Serial.println((int)body.length());

        // Count occurrences of the tag — should equal STREAM_COUNT
        // since httpbin echoes headers in each streamed record.
        int found = 0;
        int from = 0;
        const String needle = String(TEST_HEADER_VALUE);
        while (from < (int)body.length())
        {
            const int hit = body.indexOf(needle, from);
            if (hit < 0)
                break;
            ++found;
            from = hit + needle.length();
        }
        Serial.print("TAG_COUNT=");
        Serial.println(found);

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
