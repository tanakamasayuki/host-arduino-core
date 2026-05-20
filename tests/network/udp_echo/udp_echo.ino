// Bidirectional UDP echo. Exercises beginPacket / write / endPacket
// and remoteIP / remotePort round-tripping.

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

WiFiUDP udp;

void setup()
{
    Serial.begin(115200);

    if (!udp.begin(0))
    {
        Serial.println("UDP_BEGIN_FAIL");
        return;
    }
    Serial.print("UDP_PORT=");
    Serial.println(udp.localPort());
}

void loop()
{
    const int n = udp.parsePacket();
    if (n <= 0)
    {
        delay(5);
        return;
    }

    uint8_t buf[1024];
    const int got = udp.read(buf, sizeof(buf));
    if (got <= 0)
        return;

    const IPAddress src = udp.remoteIP();
    const uint16_t srcPort = udp.remotePort();

    udp.beginPacket(src, srcPort);
    udp.write(buf, got);
    udp.endPacket();

    Serial.print("ECHO ");
    Serial.println(got);
}
