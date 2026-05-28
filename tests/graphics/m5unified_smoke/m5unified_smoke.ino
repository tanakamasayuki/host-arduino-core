// Headless M5Unified smoke test.
// M5Unified wraps M5GFX as `M5.Display` (alias `M5.Lcd`).

#include <M5Unified.h>
#include <stdio.h>
#include <sys/stat.h>

static int g_frames = 0;
static constexpr int FRAME_TARGET = 20;
static bool g_captured = false;

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start m5unified_smoke");
    M5.begin();
    M5.Display.fillScreen(TFT_GREEN);
    Serial.print("size=");
    Serial.print(M5.Display.width());
    Serial.print("x");
    Serial.println(M5.Display.height());
}

void loop()
{
    if (g_frames < FRAME_TARGET)
    {
        M5.Display.fillCircle(M5.Display.width() / 2, M5.Display.height() / 2, 8 + g_frames, TFT_MAGENTA);
        g_frames++;
        return;
    }
    if (g_captured)
    {
        delay(50);
        return;
    }

    const uint32_t p_corner = M5.Display.readPixel(0, 0);
    const uint32_t p_center = M5.Display.readPixel(M5.Display.width() / 2, M5.Display.height() / 2);
    Serial.print("corner=0x");
    Serial.println(p_corner, HEX);
    Serial.print("center=0x");
    Serial.println(p_center, HEX);

    mkdir("output", 0755);

    size_t png_len = 0;
    void *png = M5.Display.createPng(&png_len, 0, 0, M5.Display.width(), M5.Display.height());
    if (png && png_len > 0)
    {
        FILE *fp = fopen("output/m5unified_smoke_capture.png", "wb");
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
