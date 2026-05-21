// Confirms [HostCore] diagnostic hints fire when WiFiUDP methods are
// invoked without a prior begin(). Used to validate the HOST_DIAG_ONCE
// channel — the user's complaint was that this misuse used to fail
// silently.

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

WiFiUDP udp; // intentionally no begin()

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start no_begin");

    // Each of these should emit one [HostCore] line on Serial.
    udp.beginPacket(IPAddress(127, 0, 0, 1), 9);
    udp.endPacket();
    udp.parsePacket();

    // Now begin properly and verify normal operation does NOT emit hints.
    udp.begin(0);
    udp.beginPacket(IPAddress(127, 0, 0, 1), udp.localPort());
    udp.write((const uint8_t *)"ok", 2);
    udp.endPacket();

    Serial.println("TEST done");
}

void loop()
{
    delay(10);
}
