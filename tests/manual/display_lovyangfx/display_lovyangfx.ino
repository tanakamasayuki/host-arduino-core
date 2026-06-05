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
    Serial.println("DISPLAY start lovyangfx");

    lcd.init();
    lcd.setRotation(HOST_DISPLAY_ROTATION);
    lcd.fillScreen(TFT_BLACK);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.setCursor(8, 8);
    lcd.println("Host Display");
    lcd.println("LovyanGFX");

    Serial.println("DISPLAY ready");
}

void loop()
{
    static uint32_t n = 0;
    lcd.fillCircle(rand() % lcd.width(), rand() % lcd.height(), 12, ++n);
    delay(100);
}
