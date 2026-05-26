// TCP echo server. Exercises WiFiServer (listen / accept) and
// WiFiClient (read / write / connected) on the accepted connection.

#include <Arduino.h>
#include <WiFi.h>

WiFiServer server(0);

void setup()
{
    Serial.begin(115200);
    server.begin();
    if (!server)
    {
        Serial.println("TCP_BEGIN_FAIL");
        return;
    }
    Serial.print("TCP_PORT=");
    Serial.println(server.port());
}

void loop()
{
    WiFiClient client = server.available();
    if (!client)
    {
        delay(5);
        return;
    }

    Serial.println("ACCEPTED");
    uint8_t buf[256];
    while (client.connected())
    {
        const int n = client.available();
        if (n <= 0)
        {
            delay(2);
            continue;
        }
        const int got = client.read(buf, sizeof(buf));
        if (got <= 0)
            break;
        client.write(buf, (size_t)got);
        Serial.print("ECHO ");
        Serial.println(got);
    }
    client.stop();
    Serial.println("CLOSED");
}
