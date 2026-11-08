#pragma once

#include "guitypes.hpp"
#include "raster_opfn.hpp"

#include <cstdint>

/// Color themes are applied by the display driver, which remaps the background, text, button and
/// brand colors of everything drawn. Grayscale icons are inverted in light themes; other colors
/// baked into images are not remapped.
namespace color_theme {

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
    _cnt,
};

/// Selects the theme the display draws with. Does not redraw anything.
void set(Theme theme);

Theme get();

/// Translates \p color from the default palette to the current theme
Color map(Color color);

/// Adjusts raster operations of a grayscale image to keep it visible in the current theme
ropfn map(ropfn rop);

} // namespace color_theme
