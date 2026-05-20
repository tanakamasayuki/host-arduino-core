// One-way UDP test: pytest sends, sketch reports payload via Serial.

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
    const uint16_t port = udp.localPort();
    Serial.print("UDP_PORT=");
    Serial.println(port);
}

void loop()
{
    const int n = udp.parsePacket();
    if (n <= 0)
    {
        delay(5);
        return;
    }

    char buf[256];
    const int got = udp.read(buf, sizeof(buf) - 1);
    buf[got > 0 ? got : 0] = '\0';

    Serial.print("RX from ");
    Serial.print(udp.remoteIP());
    Serial.print(':');
    Serial.print(udp.remotePort());
    Serial.print(' ');
    Serial.print(got);
    Serial.print(' ');
    Serial.println(buf);
}
