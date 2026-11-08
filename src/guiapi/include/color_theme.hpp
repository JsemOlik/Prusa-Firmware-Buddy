#pragma once

#include "guitypes.hpp"
#include "raster_opfn.hpp"

#include <cstdint>

/// Color themes are applied by the display driver, which remaps the colors of everything drawn
/// from the default palette. The same default color can play different roles - white is both
/// the text color and the background of a focused item - so backgrounds and foregrounds are
/// remapped separately.
namespace color_theme {

/// The order is persisted in the config store, so new themes go to the end
enum class Theme : uint8_t {
    prusa,
    ocean,
    forest,
    amethyst,
    crimson,
    gold,
    dracula,
    nord,
    cyan,
    synthwave,
    light,
    cyan_light,
    sakura,
    sakura_dark,
    _cnt,
};

/// Selects the theme the display draws with. Does not redraw anything.
void set(Theme theme);

Theme get();

/// Translates a color used to fill an area
Color map_back(Color color);

/// Translates a color used to draw text or lines
Color map_front(Color color);

/// Translates a text color, making sure it stays readable on the already translated \p back
Color map_text(Color color, Color back);

/// Adjusts the raster operations of an image drawn over \p back (not translated yet)
ropfn map(ropfn rop, Color back);

/// Translates a pixel of an image with ropfn::recolor set
void map_pixel(uint8_t &r, uint8_t &g, uint8_t &b);

} // namespace color_theme
