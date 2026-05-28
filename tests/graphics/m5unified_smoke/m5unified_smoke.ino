// m5unified_smoke
//
// Same shape as m5gfx_smoke / lovyangfx_smoke, driven by M5Unified
// (main panel via M5.Display, sprites parented to M5.Display).
// drawMain and drawHome come from the gfx_demo library.
//
// Output PNGs:
//   output/main.png             ← drawMain on the physical panel
//   output/home_<board>.png     ← drawHome on each Case[] entry

#include <M5Unified.h>
#include <gfx_demo.h>
#include <stdio.h>
#include <sys/stat.h>

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
    Serial.println("TEST start m5unified_smoke");

    mkdir("output", 0755);
    M5.begin();

    drawMain(M5.Display);
    Serial.println(save_png(M5.Display, "output/main.png") ? "MAIN ok" : "MAIN fail");

    const char *fn = "home";
    for (const auto &c : cases)
    {
        LGFX_Sprite canvas(&M5.Display);
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
