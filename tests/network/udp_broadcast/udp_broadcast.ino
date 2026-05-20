// Verifies SO_BROADCAST is enabled by WiFiUDP::begin() — sending to
// 255.255.255.255 should succeed (endPacket returns 1). Without the
// option set, Linux returns EACCES from sendto().

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

    const IPAddress bcast(255, 255, 255, 255);
    udp.beginPacket(bcast, 9);
    udp.write((const uint8_t *)"bcast", 5);
    const int sent = udp.endPacket();
    Serial.print("BCAST_SENT=");
    Serial.println(sent);

    udp.beginPacket(IPAddress(127, 0, 0, 1), udp.localPort());
    udp.write((const uint8_t *)"self", 4);
    const int self = udp.endPacket();
    Serial.print("SELF_SENT=");
    Serial.println(self);
}

void loop()
{
    const int n = udp.parsePacket();
    if (n > 0)
    {
        char buf[64];
        const int got = udp.read((unsigned char *)buf, sizeof(buf) - 1);
        buf[got > 0 ? got : 0] = '\0';
        Serial.print("RX ");
        Serial.println(buf);
    }
    delay(5);
}
