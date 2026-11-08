#include "leds/side_strip_handler.hpp"
#include <led_animation_controller/simple_transition_controller.hpp>
#include "marlin_server.hpp"

namespace leds {

static SimpleTransitionController &controller_instance() {
    static SimpleTransitionController instance;
    return instance;
}

SideStripHandler &SideStripHandler::instance() {
    static SideStripHandler instance;
    return instance;
}

SideStripHandler::SideStripHandler() {
    load_config();
}

void SideStripHandler::activity_ping() {
    std::lock_guard lock(mutex);
    active_timestamp_ms = ticks_ms();
}

void SideStripHandler::set_custom_color(ColorRGBW color, uint32_t duration_ms, uint32_t transition_ms) {
    std::lock_guard lock(mutex);
    custom_color.emplace(color, ticks_ms(), duration_ms, transition_ms);
    // Set state temporarily to off to force a change in case the state is already custom_color
    state = SideStripState::off;
}

void SideStripHandler::set_theme_color(ColorRGBW color) {
    std::lock_guard lock(mutex);
    if (theme_color == color) {
        return;
    }

    theme_color = color;
    // Set state to off to force a change of state that will transition to the new color
    state = SideStripState::off;
}

bool SideStripHandler::get_follow_theme() const {
    std::lock_guard lock(mutex);
    return follow_theme;
}

void SideStripHandler::set_follow_theme(bool value) {
    config_store().side_leds_follow_theme.set(value);
    std::lock_guard lock(mutex);
    follow_theme = value;
    state = SideStripState::off;
}

void SideStripHandler::notify_print_finished() {
    static constexpr uint32_t pulse_period_ms = 700;
    static constexpr uint32_t pulse_count = 4;

    std::lock_guard lock(mutex);
    if (!follow_theme || max_brightness == 0) {
        return;
    }
    custom_color.emplace(base_color(max_brightness), ticks_ms(), 2 * pulse_count * pulse_period_ms, pulse_period_ms / 2, pulse_period_ms);
    state = SideStripState::off;
}

void SideStripHandler::update() {
    std::lock_guard lock(mutex);
    const uint32_t time_ms = ticks_ms();

    auto &controller = controller_instance();
    if (custom_color && time_ms - custom_color->start_ms < custom_color->duration_ms) {
        if (custom_color->pulse_period_ms) {
            const bool on = (time_ms - custom_color->start_ms) / custom_color->pulse_period_ms % 2 == 0;
            if (on != custom_color->pulse_on) {
                custom_color->pulse_on = on;
                // Force a transition to the other half of the pulse
                state = SideStripState::off;
            }
        }
        change_state(SideStripState::custom_color);
    } else {
        custom_color.reset();
        const bool dont_dim = dimming_enabled == DimmingEnabled::never || (dimming_enabled == DimmingEnabled::not_printing && marlin_server::is_printing());
        if (dont_dim || (active_timestamp_ms != 0 && time_ms - active_timestamp_ms < active_timeout_ms)) {
            change_state(SideStripState::active);
        } else {
            // Clear to prevent overflow bugs
            active_timestamp_ms = 0;
            change_state(SideStripState::dimmed);
        }
    }

    controller.update();
}

void SideStripHandler::load_config() {
    std::lock_guard lock(mutex);
    dimmed_brightness = config_store().side_leds_dimmed_brightness.get();
    max_brightness = config_store().side_leds_max_brightness.get();
    dimming_enabled = config_store().side_leds_dimming_enabled.get();
    follow_theme = config_store().side_leds_follow_theme.get();
    // Set state to off to force a change of state that will transition to the new brightness
    state = SideStripState::off;
}

uint8_t SideStripHandler::get_max_brightness() const {
    std::lock_guard lock(mutex);
    return max_brightness;
}

void SideStripHandler::set_max_brightness(uint8_t value) {
    config_store().side_leds_max_brightness.set(value);
    std::lock_guard lock(mutex);
    if (max_brightness == value) {
        return;
    }

    max_brightness = value;
    // Set state to off to force a change of state that will transition to the new brightness
    state = SideStripState::off;
}

uint8_t SideStripHandler::get_dimmed_brightness() const {
    std::lock_guard lock(mutex);
    return dimmed_brightness;
}

void SideStripHandler::set_dimmed_brightness(uint8_t value) {
    config_store().side_leds_dimmed_brightness.set(value);
    std::lock_guard lock(mutex);
    if (dimmed_brightness == value) {
        return;
    }

    dimmed_brightness = value;
    // Set state to off to force a change of state that will transition to the new brightness
    state = SideStripState::off;
}

DimmingEnabled SideStripHandler::get_dimming_enabled() const {
    std::lock_guard lock(mutex);
    return dimming_enabled;
}

void SideStripHandler::set_dimming_enabled(DimmingEnabled value) {
    config_store().side_leds_dimming_enabled.set(value);
    std::lock_guard lock(mutex);
    dimming_enabled = value;
}

leds::ColorRGBW SideStripHandler::color() const {
    std::lock_guard lock(mutex);
    return controller_instance().color();
}

bool SideStripHandler::is_dimmed() const {
    std::lock_guard lock(mutex);
    return state == SideStripState::dimmed;
}

void SideStripHandler::change_state(SideStripState state) {
    if (this->state != state) {
        this->state = state;

        uint32_t duration;
        if (state == SideStripState::dimmed) {
            duration = 5000;
        } else if (state == SideStripState::custom_color) {
            duration = custom_color->transition_ms;
        } else {
            duration = 500;
        }
        controller_instance().set(get_color_for_state(state), duration);
    }
}

ColorRGBW SideStripHandler::base_color(uint8_t level) const {
    if (follow_theme) {
        const auto scale = [level](uint8_t channel) {
            return static_cast<uint8_t>(channel * level / 255);
        };
        return ColorRGBW(scale(theme_color.r), scale(theme_color.g), scale(theme_color.b));
    }

    constexpr auto white = has_white_led() ? ColorRGBW(0, 0, 0, 255) : ColorRGBW(255, 255, 255);
    return white.clamp(level);
}

ColorRGBW SideStripHandler::get_color_for_state(SideStripState state) {
    switch (state) {
    case SideStripState::off:
        return ColorRGBW();
    case SideStripState::dimmed:
        return base_color(dimmed_brightness);
    case SideStripState::active:
        return base_color(max_brightness);
    case SideStripState::custom_color:
        return custom_color->pulse_on ? custom_color->color : ColorRGBW();
    }
    return {};
}

} // namespace leds
