// Ordinary Arduino-style sketch with M5GFX on host_lgfx core.
// SDL is headless (driver=dummy); Serial runs over TCP via HostRuntime
// so the pytest dut fixture can observe progress and assert outputs.
//
// File I/O convention:
//   - CWD at runtime is the sketch directory (pytest-embedded behavior).
//   - Sketches that write artifacts should put them under `output/`.
//   - conftest.py wipes `output/` before each test; the sketch (re)creates
//     it on the fly when it needs to write something.
//   - `output/` is in the repo's .gitignore so artifacts never get
//     committed.

#include <M5GFX.h>
#include <stdio.h>
#include <sys/stat.h>

static M5GFX gfx;
static int g_frames = 0;
static constexpr int FRAME_TARGET = 20;
static bool g_captured = false;

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start m5gfx_smoke");
    gfx.init();
    gfx.fillScreen(TFT_BLUE);
    Serial.print("size=");
    Serial.print(gfx.width());
    Serial.print("x");
    Serial.println(gfx.height());
}

void loop()
{
    if (g_frames < FRAME_TARGET)
    {
        gfx.fillCircle(gfx.width() / 2, gfx.height() / 2, 8 + g_frames, TFT_YELLOW);
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

    mkdir("output", 0755); // ignore failure: already exists is fine

    size_t png_len = 0;
    void *png = gfx.createPng(&png_len, 0, 0, gfx.width(), gfx.height());
    if (png && png_len > 0)
    {
        FILE *fp = fopen("output/m5gfx_smoke_capture.png", "wb");
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
