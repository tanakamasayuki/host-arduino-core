// Multi-request HTTP probe. Reads "GET <url>" lines from Serial; for
// each line, fires an HTTP request via HTTPClient and prints status
// code, declared content length, and body back over Serial.
//
// Stays in loop() so a single test can exercise several response
// shapes (Content-Length, chunked, etc.) without restarting the sketch.

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

HTTPClient http;
static String pending_line;

static String poll_line()
{
    while (Serial.available() > 0)
    {
        const int c = Serial.read();
        if (c < 0)
            break;
        if (c == '\n' || c == '\r')
        {
            if (pending_line.length() > 0)
            {
                String ret = pending_line;
                pending_line = "";
                return ret;
            }
            continue;
        }
        pending_line += (char)c;
    }
    return String();
}

void setup()
{
    Serial.begin(115200);
    Serial.println("READY");
}

void loop()
{
    const String line = poll_line();
    if (line.length() == 0)
    {
        delay(5);
        return;
    }
    if (!line.startsWith("GET "))
    {
        Serial.print("BAD_LINE:");
        Serial.println(line);
        return;
    }
    const String url = line.substring(4);

    if (!http.begin(url))
    {
        Serial.println("BEGIN_FAIL");
        return;
    }
    http.addHeader("X-Test", "host-arduino-core");

    const int code = http.GET();
    Serial.print("CODE=");
    Serial.println(code);
    Serial.print("LEN=");
    Serial.println(http.getSize());

    if (code > 0)
    {
        const String body = http.getString();
        Serial.print("BODY_LEN=");
        Serial.println((int)body.length());
        Serial.print("BODY:");
        Serial.println(body);
    }
    http.end();
    Serial.println("DONE");
}
