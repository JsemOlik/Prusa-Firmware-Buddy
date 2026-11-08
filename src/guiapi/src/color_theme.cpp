#include "color_theme.hpp"

#include <array>
#include <utility>

namespace color_theme {

namespace {

    struct Palette {
        Color background;
        Color text;
        Color accent;
        Color button;
        bool light;
    };

    constexpr Color rgb(uint32_t raw) {
        return Color::from_raw(raw);
    }

    // The theme colors must differ from the remapped ones (unless mapped to themselves), so that
    // remapping is idempotent - some drawing primitives pass already remapped colors to others
    constexpr std::array<Palette, std::to_underlying(Theme::_cnt)> palettes { {
        { COLOR_BLACK, COLOR_WHITE, COLOR_BRAND, COLOR_DARK_GRAY, false },
        { rgb(0x07182B), COLOR_WHITE, rgb(0x29B6F6), rgb(0x1E3A5C), false },
        { rgb(0x0B1A0E), COLOR_WHITE, rgb(0x5CC95F), rgb(0x24452B), false },
        { rgb(0x170D26), COLOR_WHITE, rgb(0xB388FF), rgb(0x3A2A57), false },
        { rgb(0x1E0808), COLOR_WHITE, rgb(0xE53935), rgb(0x4A1A1A), false },
        { rgb(0x15110A), COLOR_WHITE, rgb(0xFFC107), rgb(0x4A3B14), false },
        { rgb(0x282A36), rgb(0xF8F8F2), rgb(0xBD93F9), rgb(0x44475A), false },
        { rgb(0x2E3440), rgb(0xECEFF4), rgb(0x88C0D0), rgb(0x4C566A), false },
        { rgb(0x001A1F), COLOR_WHITE, rgb(0x00E5FF), rgb(0x0F4A55), false },
        { rgb(0x1A1033), COLOR_WHITE, rgb(0xFF2E97), rgb(0x3D2466), false },
        { rgb(0xF2F2F2), rgb(0x1A1A1A), rgb(0xE0561A), rgb(0xC9C9C9), true },
        { rgb(0xF0FBFC), rgb(0x0B2A30), rgb(0x0097A7), rgb(0xBFE6EA), true },
        { rgb(0xFFF0F5), rgb(0x3A1F2B), rgb(0xE91E63), rgb(0xF3C6D6), true },
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
    } else if (color == COLOR_WHITE) {
        return palette.text;
    } else if (color == COLOR_BRAND) {
        return palette.accent;
    } else if (color == COLOR_DARK_GRAY || color == COLOR_GRAY) {
        return palette.button;
    }
    return color;
}

ropfn map(ropfn rop) {
    if (palettes[std::to_underlying(current)].light) {
        rop.swap_bw = rop.swap_bw == has_swapped_bw::yes ? has_swapped_bw::no : has_swapped_bw::yes;
    }
    return rop;
}

} // namespace color_theme
