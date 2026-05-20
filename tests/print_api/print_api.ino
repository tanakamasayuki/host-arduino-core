// Tests for Print / Serial formatting helpers.
//
// Drives Serial.print/println with the common Arduino overloads
// (integers in DEC/HEX/BIN, floats, String, C string, bool).

#include <Arduino.h>

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start print");

    Serial.print("int:");
    Serial.println(42);

    Serial.print("neg:");
    Serial.println(-7);

    Serial.print("hex:");
    Serial.println(0xAB, HEX);

    Serial.print("bin:");
    Serial.println(0b1011, BIN);

    Serial.print("float:");
    Serial.println(3.14, 2);

    Serial.print("cstr:");
    Serial.println("hello");

    String s = "wo";
    s += "rld";
    Serial.print("string:");
    Serial.println(s);

    Serial.print("bool:");
    Serial.println(true);

    Serial.println("TEST done");
}

void loop()
{
    delay(10);
}
