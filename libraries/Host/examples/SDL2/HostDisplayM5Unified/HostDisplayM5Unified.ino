#include <M5Unified.h>

void setup()
{
    Serial.begin(115200);
    Serial.println("HostDisplayM5Unified start");
    M5_LOGE("HostDisplayM5Unified M5_LOGE sample");

    M5.begin();
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(8, 8);
    M5.Display.println("Host Display");
    M5.Display.println("M5Unified");

    Serial.println("HostDisplayM5Unified ready");
}

void loop()
{
    static uint32_t frame = 0;
    const int32_t x = (frame * 7) % M5.Display.width();
    const int32_t y = (frame * 5) % M5.Display.height();
    M5.Display.fillCircle(x, y, 10, M5.Display.color888((frame * 3) & 0xff, (frame * 5) & 0xff, (frame * 11) & 0xff));
    ++frame;

    M5.update();
    delay(50);
}
