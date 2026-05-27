// Exercises HTTPClient::setFollowRedirects() over plain HTTP.
//
// Protocol over Serial (one command per line):
//   MODE <0|1|2>       set follow-redirects mode (DISABLE/STRICT/FORCE)
//   LIMIT <n>          set redirect limit
//   GET <url>          issue request and report status / location / body

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
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.setRedirectLimit(10);
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

    if (line.startsWith("MODE "))
    {
        const int v = line.substring(5).toInt();
        http.setFollowRedirects((followRedirects_t)v);
        Serial.print("MODE_OK ");
        Serial.println(v);
        return;
    }
    if (line.startsWith("LIMIT "))
    {
        const int v = line.substring(6).toInt();
        http.setRedirectLimit((uint16_t)v);
        Serial.print("LIMIT_OK ");
        Serial.println(v);
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
    Serial.print("LOC=");
    Serial.println(http.getLocation());
    if (code > 0)
    {
        const String body = http.getString();
        Serial.print("BODY:");
        Serial.println(body);
    }
    http.end();
    Serial.println("DONE");
}
