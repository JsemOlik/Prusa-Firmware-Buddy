#include "screen_menu_motion_limits.hpp"

#include "ScreenHandler.hpp"
#include "img_resources.hpp"
#include "marlin_client.hpp"
#include <config_store/store_instance.hpp>
#include <inc/MarlinConfigPre.h>
#include <window_msgbox.hpp>

#include <array>

namespace {

constexpr uint32_t default_accel = std::to_array<uint32_t>(DEFAULT_MAX_ACCELERATION)[0];
constexpr uint16_t default_feedrate = std::to_array<uint16_t>(DEFAULT_MAX_FEEDRATE)[0];

constexpr NumericInputConfig accel_config {
    .min_value = 1000,
    .max_value = std::to_array<uint32_t>(HWLIMIT_NORMAL_MAX_ACCELERATION)[0],
    .step = 500,
};

constexpr NumericInputConfig feedrate_config {
    .min_value = 50,
    .max_value = std::to_array<uint16_t>(HWLIMIT_NORMAL_MAX_FEEDRATE)[0],
    .step = 10,
};

uint32_t max_acceleration_xy() {
    return config_store().max_acceleration_xy.get() ?: default_accel;
}

uint16_t max_feedrate_xy() {
    return config_store().max_feedrate_xy.get() ?: default_feedrate;
}

void set_max_feedrate_xy(uint16_t feedrate) {
    config_store().max_feedrate_xy.set(feedrate == default_feedrate ? 0 : feedrate);
    marlin_client::gcode_printf("M203 X%u Y%u", feedrate, feedrate);
}

} // namespace

void motion_limits::set_max_acceleration_xy(uint32_t accel) {
    config_store().max_acceleration_xy.set(accel == default_accel ? 0 : accel);
    marlin_client::gcode_printf("M201 X%lu Y%lu", static_cast<unsigned long>(accel), static_cast<unsigned long>(accel));
}

MI_MAX_ACCELERATION_XY::MI_MAX_ACCELERATION_XY()
    : WiSpin(max_acceleration_xy(), accel_config, _(label)) {}

void MI_MAX_ACCELERATION_XY::reload() {
    set_value(max_acceleration_xy());
}

void MI_MAX_ACCELERATION_XY::OnClick() {
    motion_limits::set_max_acceleration_xy(static_cast<uint32_t>(value()));
}

MI_MAX_FEEDRATE_XY::MI_MAX_FEEDRATE_XY()
    : WiSpin(max_feedrate_xy(), feedrate_config, _(label)) {}

void MI_MAX_FEEDRATE_XY::reload() {
    set_value(max_feedrate_xy());
}

void MI_MAX_FEEDRATE_XY::OnClick() {
    set_max_feedrate_xy(static_cast<uint16_t>(value()));
}

MI_MOTION_LIMITS_DEFAULTS::MI_MOTION_LIMITS_DEFAULTS()
    : IWindowMenuItem(_(label)) {}

void MI_MOTION_LIMITS_DEFAULTS::click(IWindowMenu & /*window_menu*/) {
    motion_limits::set_max_acceleration_xy(default_accel);
    set_max_feedrate_xy(default_feedrate);
    Screens::Access()->WindowEvent(GUI_event_t::CHILD_CLICK, this);
}

ScreenMenuMotionLimits::ScreenMenuMotionLimits()
    : ScreenMenuMotionLimitsSpec(_(label)) {
    header.SetIcon(&img::calibrate_white_16x16);
}

void ScreenMenuMotionLimits::windowEvent(window_t *sender, GUI_event_t event, void *param) {
    if (event == GUI_event_t::CHILD_CLICK) {
        Item<MI_MAX_ACCELERATION_XY>().reload();
        Item<MI_MAX_FEEDRATE_XY>().reload();
        return;
    }
    ScreenMenu::windowEvent(sender, event, param);
}
