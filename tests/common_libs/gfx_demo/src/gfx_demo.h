#pragma once

// gfx_demo.h
//
// Declarations only. The caller must include their graphics library
// (one of M5Unified.h / M5GFX.h / LovyanGFX.hpp) BEFORE this header so
// the `LovyanGFX` type is in scope — gfx_demo.h is intentionally
// neutral and does NOT pull a gfx library itself.
//
// The 3-way `__has_include` switch lives in gfx_demo.cpp only; see
// that file for the rationale and for why production code should not
// copy that switch verbatim.

// drawMain: panel-sanity test pattern (border + central box + dot).
// Used as the smoke's main-panel render to verify init + readback
// without touching drawHome's layout.
void drawMain(LovyanGFX &gfx);

// drawHome: layout-agnostic content render (top bar + central circle +
// 4 corner markers). Exercised across many sizes / rotations / color
// depths via LGFX_Sprite from each smoke sketch.
void drawHome(LovyanGFX &gfx);
