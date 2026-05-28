// Headless LovyanGFX smoke test.
// Uses LGFX_AUTODETECT which on host (with SDL2) instantiates an LGFX class
// backed by Panel_sdl.

#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
#include <stdio.h>
#include <sys/stat.h>

static LGFX gfx;
static int g_frames = 0;
static constexpr int FRAME_TARGET = 20;
static bool g_captured = false;

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start lovyangfx_smoke");
    gfx.init();
    gfx.fillScreen(TFT_CYAN);
    Serial.print("size=");
    Serial.print(gfx.width());
    Serial.print("x");
    Serial.println(gfx.height());
}

void loop()
{
    if (g_frames < FRAME_TARGET)
    {
        gfx.fillCircle(gfx.width() / 2, gfx.height() / 2, 8 + g_frames, TFT_ORANGE);
        g_frames++;
        return;
    }
    if (g_captured)
    {
        delay(50);
        return;
    }

    const uint32_t p_corner = gfx.readPixel(0, 0);
    const uint32_t p_center = gfx.readPixel(gfx.width() / 2, gfx.height() / 2);
    Serial.print("corner=0x");
    Serial.println(p_corner, HEX);
    Serial.print("center=0x");
    Serial.println(p_center, HEX);

    mkdir("output", 0755);

    size_t png_len = 0;
    void *png = gfx.createPng(&png_len, 0, 0, gfx.width(), gfx.height());
    if (png && png_len > 0)
    {
        FILE *fp = fopen("output/lovyangfx_smoke_capture.png", "wb");
        if (fp)
        {
            fwrite(png, 1, png_len, fp);
            fclose(fp);
            Serial.print("CAPTURE bytes=");
            Serial.println(png_len);
        }
        else
        {
            Serial.println("CAPTURE open_failed");
        }
        free(png);
    }
    else
    {
        Serial.println("CAPTURE failed");
    }
    Serial.println("TEST done");
    g_captured = true;
}
