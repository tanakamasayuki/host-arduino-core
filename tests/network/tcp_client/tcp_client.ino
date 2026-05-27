// TCP client. Reads "CONNECT <port>" from Serial, connects to
// 127.0.0.1:<port>, sends a payload, then echoes whatever bytes the
// peer returns until it closes the connection.

#include <Arduino.h>
#include <WiFi.h>

WiFiClient client;

static String readLine()
{
    String out;
    // 30s gives pytest enough headroom even when this test runs as part
    // of a long suite where launcher startup and TCP setup accumulate
    // delay before the CONNECT line arrives.
    const uint32_t deadline = millis() + 30000;
    while (millis() < deadline)
    {
        const int c = Serial.read();
        if (c < 0)
        {
            delay(2);
            continue;
        }
        if (c == '\n' || c == '\r')
        {
            if (out.length() > 0)
                return out;
            continue;
        }
        out += (char)c;
    }
    return out;
}

void setup()
{
    Serial.begin(115200);
    Serial.println("READY");

    const String line = readLine();
    if (!line.startsWith("CONNECT "))
    {
        Serial.print("BAD_LINE:");
        Serial.println(line);
        return;
    }
    const uint16_t port = (uint16_t)line.substring(8).toInt();
    Serial.print("CONNECTING ");
    Serial.println(port);

    if (!client.connect(IPAddress(127, 0, 0, 1), port))
    {
        Serial.print("CONNECT_FAIL ");
        Serial.println(client.lastError());
        return;
    }
    Serial.println("CONNECTED");

    const char *msg = "PING";
    const size_t wrote = client.write((const uint8_t *)msg, 4);
    Serial.print("WROTE ");
    Serial.println((int)wrote);
}

void loop()
{
    if (!client)
    {
        delay(20);
        return;
    }
    while (client.connected())
    {
        const int n = client.available();
        if (n <= 0)
        {
            delay(2);
            continue;
        }
        uint8_t buf[64];
        const int got = client.read(buf, sizeof(buf));
        if (got <= 0)
            break;
        Serial.print("RX ");
        for (int i = 0; i < got; ++i)
            Serial.write(buf[i]);
        Serial.println();
    }
    client.stop();
    Serial.println("CLOSED");
    delay(1000);
}
