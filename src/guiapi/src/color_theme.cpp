#include "color_theme.hpp"

#include <array>
#include <cstdlib>
#include <utility>

namespace color_theme {

namespace {

    struct Palette {
        /// Replaces black backgrounds
        Color background;

        /// Replaces gray backgrounds: icon tiles, buttons
        Color surface;

        /// Replaces white text
        Color text;

        /// Replaces white backgrounds: focused items
        Color focus;

        /// Replaces black text: text on focused items and on accent
        Color focus_text;

        /// Replaces the brand color, including in icons
        Color accent;
    };

    constexpr Color rgb(uint32_t raw) {
        return Color::from_raw(raw);
    }

    constexpr std::array<Palette, std::to_underlying(Theme::custom)> palettes { {
        // prusa (not remapped at all)
        { COLOR_BLACK, COLOR_DARK_GRAY, COLOR_WHITE, COLOR_WHITE, COLOR_BLACK, COLOR_BRAND },
        // ocean
        { rgb(0x07182B), rgb(0x1E3A5C), rgb(0xE3F2FD), rgb(0x29B6F6), rgb(0x07182B), rgb(0x29B6F6) },
        // forest
        { rgb(0x0B1A0E), rgb(0x24452B), rgb(0xE8F5E9), rgb(0x5CC95F), rgb(0x0B1A0E), rgb(0x5CC95F) },
        // amethyst
        { rgb(0x170D26), rgb(0x3A2A57), rgb(0xF3E5F5), rgb(0xB388FF), rgb(0x170D26), rgb(0xB388FF) },
        // crimson
        { rgb(0x1E0808), rgb(0x4A1A1A), rgb(0xFFEBEE), rgb(0xE53935), rgb(0xFFF5F5), rgb(0xE53935) },
        // gold
        { rgb(0x15110A), rgb(0x4A3B14), rgb(0xFFF8E1), rgb(0xFFC107), rgb(0x15110A), rgb(0xFFC107) },
        // dracula (Sweet Dracula)
        { rgb(0x161722), rgb(0x2F2F4A), rgb(0xF8F8F2), rgb(0xBD93F9), rgb(0x161722), rgb(0xFF79C6) },
        // nord
        { rgb(0x2E3440), rgb(0x434C5E), rgb(0xECEFF4), rgb(0x88C0D0), rgb(0x2E3440), rgb(0x88C0D0) },
        // cyan
        { rgb(0x001A1F), rgb(0x0F4A55), rgb(0xE0F7FA), rgb(0x00E5FF), rgb(0x001A1F), rgb(0x00E5FF) },
        // synthwave
        { rgb(0x1A1033), rgb(0x3D2466), rgb(0xF5E6FF), rgb(0x36F9F6), rgb(0x1A1033), rgb(0xFF2E97) },
        // light
        { rgb(0xF2F2F2), rgb(0x5A5A5A), rgb(0x1A1A1A), rgb(0x1A1A1A), rgb(0xF2F2F2), rgb(0xE0561A) },
        // cyan_light
        { rgb(0xFAFAFA), rgb(0x575757), rgb(0x141414), rgb(0x007A6C), rgb(0xFAFAFA), rgb(0x007A6C) },
        // sakura
        { rgb(0xFFF0F5), rgb(0x8E5A6E), rgb(0x3A1F2B), rgb(0xE91E63), rgb(0xFFF0F5), rgb(0xE91E63) },
        // sakura_dark
        { rgb(0x1F1418), rgb(0x4A2C38), rgb(0xFFE4EC), rgb(0xF48FB1), rgb(0x1F1418), rgb(0xF48FB1) },
    } };

    /// Mixes \p amount / 256 of \p color into \p base
    constexpr Color mix(Color base, Color color, uint16_t amount) {
        const auto channel = [&](uint8_t a, uint8_t b) {
            return static_cast<uint8_t>((a * (256 - amount) + b * amount) >> 8);
        };
        return Color::from_rgb(channel(base.r, color.r), channel(base.g, color.g), channel(base.b, color.b));
    }

    /// A dark theme tinted with the accent
    Palette derive_palette(Color accent) {
        const Color background = mix(COLOR_BLACK, accent, 24);
        return {
            .background = background,
            .surface = mix(mix(COLOR_BLACK, COLOR_GRAY, 96), accent, 72),
            .text = mix(COLOR_WHITE, accent, 24),
            .focus = accent,
            .focus_text = accent.to_grayscale() >= 128 ? background : COLOR_WHITE,
            .accent = accent,
        };
    }

    Theme current = Theme::prusa;
    Palette custom_palette = derive_palette(COLOR_BRAND);

    const Palette &palette() {
        return current == Theme::custom ? custom_palette : palettes[std::to_underlying(current)];
    }

    bool is_light(Color color) {
        return color.to_grayscale() >= 128;
    }

    /// Text closer than this in luminance to its background is hard to read
    constexpr int min_text_contrast = 96;

    int contrast(Color a, Color b) {
        return std::abs(int(a.to_grayscale()) - int(b.to_grayscale()));
    }

    /// The brand orange as used in the icons, including its antialiased edges
    bool is_brand_pixel(uint8_t r, uint8_t g, uint8_t b) {
        return r >= 200 && g >= 60 && g <= 150 && b <= 90 && r - g >= 90;
    }

} // namespace

void set(Theme theme) {
    current = theme < Theme::_cnt ? theme : Theme::prusa;
}

Theme get() {
    return current;
}

void set_custom_accent(Color accent) {
    custom_palette = derive_palette(accent);
}

Color accent() {
    return palette().accent;
}

Color map_back(Color color) {
    if (current == Theme::prusa) {
        return color;
    }

    const Palette &p = palette();
    if (color == COLOR_BLACK) {
        return p.background;
    } else if (color == COLOR_WHITE) {
        return p.focus;
    } else if (color == COLOR_BRAND) {
        return p.accent;
    } else if (color == COLOR_DARK_GRAY || color == COLOR_GRAY) {
        return p.surface;
    }
    return color;
}

Color map_front(Color color) {
    if (current == Theme::prusa) {
        return color;
    }

    const Palette &p = palette();
    if (color == COLOR_WHITE) {
        return p.text;
    } else if (color == COLOR_BLACK) {
        return p.focus_text;
    } else if (color == COLOR_BRAND) {
        return p.accent;
    }
    return color;
}

Color map_text(Color color, Color back) {
    const Color mapped = map_front(color);
    if (current == Theme::prusa || contrast(mapped, back) >= min_text_contrast) {
        return mapped;
    }

    const Palette &p = palette();
    return contrast(p.text, back) >= contrast(p.background, back) ? p.text : p.background;
}

ropfn map(ropfn rop, Color back) {
    if (current == Theme::prusa) {
        return rop;
    }

    // Grayscale icons are designed for the default background; invert them when the theme
    // turns a dark background light or vice versa
    if (is_light(back) != is_light(map_back(back))) {
        rop.swap_bw = rop.swap_bw == has_swapped_bw::yes ? has_swapped_bw::no : has_swapped_bw::yes;
    }
    rop.recolor = true;
    return rop;
}

void map_pixel(uint8_t &r, uint8_t &g, uint8_t &b) {
    if (current == Theme::prusa || !is_brand_pixel(r, g, b)) {
        return;
    }

    const Color accent = palette().accent;
    r = accent.r;
    g = accent.g;
    b = accent.b;
}

} // namespace color_theme
