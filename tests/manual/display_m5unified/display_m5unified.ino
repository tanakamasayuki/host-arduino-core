#include <M5Unified.h>

void setup()
{
    Serial.begin(115200);
    Serial.println("DISPLAY start m5unified");
    M5_LOGE("DISPLAY m5 log error");

    M5.begin();
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setCursor(8, 8);
    M5.Display.println("Host Display");
    M5.Display.println("M5Unified");

    Serial.println("DISPLAY ready");
}

void loop()
{
    M5.update();
    delay(16);
}
