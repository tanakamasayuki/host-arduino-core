// HTTPClient over TLS. Reads "GET <https-url>" lines from Serial, fires
// the request via the internal WiFiClientSecure (enabled by the
// tls=openssl board menu option), and prints the result.

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
