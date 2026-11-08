#pragma once

#include "guitypes.hpp"

#include <cstdint>

/// Color themes are applied by the display driver, which remaps the background and the brand
/// color of everything drawn. Colors baked into images are not remapped.
namespace color_theme {

enum class Theme : uint8_t {
    prusa,
    ocean,
    forest,
    amethyst,
    crimson,
    gold,
    _cnt,
};

/// Selects the theme the display draws with. Does not redraw anything.
void set(Theme theme);

Theme get();

/// Translates \p color from the default palette to the current theme
Color map(Color color);

} // namespace color_theme
