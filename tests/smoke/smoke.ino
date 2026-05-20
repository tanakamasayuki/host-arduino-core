// Smoke test sketch — verifies host-arduino-core builds and the
// Serial-over-TCP harness works end-to-end against the locally installed
// (sketchbook hardware/) platform.

#include <Arduino.h>

void setup()
{
    Serial.begin(115200);
    Serial.println("SMOKE ready");
    Serial.print("millis=");
    Serial.println(millis());
}

void loop()
{
    delay(10);
}
