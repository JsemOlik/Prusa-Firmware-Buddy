#include "color_theme.hpp"

#include <array>
#include <utility>

namespace color_theme {

namespace {

    struct Palette {
        Color background;
        Color accent;
    };

    // The theme colors must differ from the remapped ones, so that remapping is idempotent
    // (some drawing primitives pass already remapped colors to others)
    constexpr std::array<Palette, std::to_underlying(Theme::_cnt)> palettes { {
        { COLOR_BLACK, COLOR_BRAND },
        { Color::from_raw(0x07182B), Color::from_raw(0x29B6F6) },
        { Color::from_raw(0x0B1A0E), Color::from_raw(0x5CC95F) },
        { Color::from_raw(0x170D26), Color::from_raw(0xB388FF) },
        { Color::from_raw(0x1E0808), Color::from_raw(0xE53935) },
        { Color::from_raw(0x15110A), Color::from_raw(0xFFC107) },
    } };

    Theme current = Theme::prusa;

} // namespace

void set(Theme theme) {
    current = theme < Theme::_cnt ? theme : Theme::prusa;
}

Theme get() {
    return current;
}

Color map(Color color) {
    if (current == Theme::prusa) {
        return color;
    }

    const Palette &palette = palettes[std::to_underlying(current)];
    if (color == COLOR_BLACK) {
        return palette.background;
    } else if (color == COLOR_BRAND) {
        return palette.accent;
    }
    return color;
}

} // namespace color_theme
