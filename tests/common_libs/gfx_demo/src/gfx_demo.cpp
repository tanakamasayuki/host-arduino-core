// gfx_demo.cpp
//
// The same `__has_include` chain as gfx_demo.h, kept in the .cpp so
// the implementation translation unit can resolve the LovyanGFX class.
//
// IMPORTANT — this 3-way `__has_include` switch only exists because the
// host-arduino-core repo wants the same draw routine to compile against
// three different upstream libraries (M5Unified / M5GFX / LovyanGFX)
// from three separate smoke tests, each declaring a different library
// in its sketch.yaml. Real application code is normally written against
// one of these libraries and should just `#include <M5GFX.h>` (or
// whichever one) directly. Copying this 3-way switch into production
// code adds unnecessary fragility — when multiple libraries are
// installed globally (e.g. in an Arduino IDE sketchbook) the
// `__has_include` order can resolve to the wrong one.

#if __has_include(<M5Unified.h>)
#include <M5Unified.h>
#elif __has_include(<M5GFX.h>)
#include <M5GFX.h>
#elif __has_include(<LovyanGFX.hpp>)
#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>
#else
#error "gfx_demo: link one of M5Unified / M5GFX / LovyanGFX"
#endif

#include "gfx_demo.h"

void drawMain(LovyanGFX &gfx)
{
    const int W = gfx.width();
    const int H = gfx.height();

    gfx.fillScreen(TFT_DARKGREEN);
    gfx.drawRect(0, 0, W, H, TFT_WHITE);
    gfx.fillRect(W / 4, H / 4, W / 2, H / 2, TFT_RED);

    const int short_side = (W < H ? W : H);
    gfx.fillCircle(W / 2, H / 2, short_side / 8, TFT_YELLOW);
}

void drawHome(LovyanGFX &gfx)
{
    const int W = gfx.width();
    const int H = gfx.height();

    gfx.fillScreen(TFT_NAVY);

    int bar_h = H / 6;
    if (bar_h < 14)
        bar_h = 14;
    gfx.fillRect(0, 0, W, bar_h, TFT_ORANGE);

    const int short_side = (W < H ? W : H);
    const int r = short_side / 4;
    gfx.fillCircle(W / 2, H / 2, r, TFT_YELLOW);

    // Corner markers (TL=red, TR=green, BL=blue, BR=white) so that
    // rotation / mirroring regressions are obvious in the captured PNG.
    const int m = 4;
    gfx.fillRect(0, 0, m, m, TFT_RED);
    gfx.fillRect(W - m, 0, m, m, TFT_GREEN);
    gfx.fillRect(0, H - m, m, m, TFT_BLUE);
    gfx.fillRect(W - m, H - m, m, m, TFT_WHITE);
}
