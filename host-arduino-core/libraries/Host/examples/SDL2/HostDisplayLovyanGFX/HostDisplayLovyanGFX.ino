#define LGFX_AUTODETECT
#include <LovyanGFX.h>
#include <LGFX_AUTODETECT.hpp>

#ifndef HOST_DISPLAY_WIDTH
#define HOST_DISPLAY_WIDTH 320
#endif

#ifndef HOST_DISPLAY_HEIGHT
#define HOST_DISPLAY_HEIGHT 240
#endif

#ifndef HOST_DISPLAY_SCALE
#define HOST_DISPLAY_SCALE 2
#endif

#ifndef HOST_DISPLAY_ROTATION
#define HOST_DISPLAY_ROTATION 0
#endif

#if defined(SDL_h_)
static LGFX lcd(HOST_DISPLAY_WIDTH, HOST_DISPLAY_HEIGHT, HOST_DISPLAY_SCALE);
#else
static LGFX lcd;
#endif

void setup()
{
    Serial.begin(115200);
    Serial.println("HostDisplayLovyanGFX start");

    lcd.init();
    lcd.setRotation(HOST_DISPLAY_ROTATION);
    lcd.fillScreen(TFT_BLACK);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.setTextSize(2);
    lcd.setCursor(8, 8);
    lcd.println("Host Display");
    lcd.println("LovyanGFX");
    lcd.printf("%dx%d scale %d\n", HOST_DISPLAY_WIDTH, HOST_DISPLAY_HEIGHT, HOST_DISPLAY_SCALE);

    Serial.println("HostDisplayLovyanGFX ready");
}

void loop()
{
    static uint32_t frame = 0;
    const int32_t x = (frame * 7) % lcd.width();
    const int32_t y = (frame * 5) % lcd.height();
    lcd.fillCircle(x, y, 10, lcd.color888((frame * 3) & 0xff, (frame * 5) & 0xff, (frame * 11) & 0xff));
    ++frame;
    delay(50);
}
