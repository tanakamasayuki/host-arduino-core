// lovyangfx_smoke
//
// Same shape as m5gfx_smoke / m5unified_smoke, driven by raw LovyanGFX
// (LGFX from LGFX_AUTODETECT, no M5GFX / M5Unified). drawMain and
// drawHome come from the gfx_demo library so regressions in either
// surface against all three smoke variants.
//
// Output PNGs:
//   output/main.png             ← drawMain on the physical panel
//   output/home_<board>.png     ← drawHome on each Case[] entry

#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
#include <gfx_demo.h>
#include <stdio.h>
#include <sys/stat.h>

static LGFX gfx;

struct Case
{
    const char *name;
    int w, h;
    uint8_t rotation;
    lgfx::color_depth_t depth;
};

static const Case cases[] = {
    {"stickcplus_p", 135, 240, 0, lgfx::rgb565_2Byte}, // M5StickCPlus 縦持ち
    {"stickcplus_l", 135, 240, 1, lgfx::rgb565_2Byte}, // M5StickCPlus 横向き (setRotation=1)
    {"core2", 320, 240, 0, lgfx::rgb565_2Byte},        // M5Stack Core2 横長
    {"atoms3", 128, 128, 0, lgfx::rgb565_2Byte},       // M5AtomS3 正方形
    {"coreink", 200, 200, 0, lgfx::grayscale_8bit},    // M5StackCoreInk e-paper 8bit grayscale
};

static bool save_png(LovyanGFX &src, const char *path)
{
    size_t len = 0;
    void *png = src.createPng(&len, 0, 0, src.width(), src.height());
    if (!png || len == 0)
        return false;
    FILE *fp = fopen(path, "wb");
    bool ok = false;
    if (fp)
    {
        ok = (fwrite(png, 1, len, fp) == len);
        fclose(fp);
    }
    free(png);
    return ok;
}

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start lovyangfx_smoke");

    mkdir("output", 0755);
    gfx.init();

    drawMain(gfx);
    Serial.println(save_png(gfx, "output/main.png") ? "MAIN ok" : "MAIN fail");

    const char *fn = "home";
    for (const auto &c : cases)
    {
        LGFX_Sprite canvas(&gfx);
        canvas.setColorDepth(c.depth);
        canvas.createSprite(c.w, c.h);
        canvas.setRotation(c.rotation);
        drawHome(canvas);
        char path[64];
        snprintf(path, sizeof(path), "output/%s_%s.png", fn, c.name);
        const bool ok = save_png(canvas, path);
        Serial.print(ok ? "CASE " : "FAIL ");
        Serial.print(fn);
        Serial.print('_');
        Serial.println(c.name);
        canvas.deleteSprite();
    }

    Serial.println("TEST done");
}

void loop()
{
    delay(1000);
}
