// interop/smoke — confirms a sketch using only the Arduino runtime
// compiles and boots on both host and ESP32 profiles.
//
// The 5-second delay after Serial.begin is required on real ESP32
// silicon to give the USB-Serial bridge time to settle; it is harmless
// (just `std::this_thread::sleep_for`) on the host runtime so the same
// sketch source works on both targets without #ifdef.

#include <Arduino.h>

void setup()
{
    Serial.begin(115200);
    delay(5000);
    Serial.println("INTEROP_SMOKE_READY");
    Serial.print("millis=");
    Serial.println(millis());
}

void loop()
{
    delay(1000);
}
