#pragma once

#include "screen_menu.hpp"
#include <WindowMenuItems.hpp>
#include <WindowMenuSpin.hpp>

namespace motion_limits {

/// Saves the XY max acceleration override and applies it right away
void set_max_acceleration_xy(uint32_t accel);

} // namespace motion_limits

class MI_MAX_ACCELERATION_XY : public WiSpin {
    static constexpr const char *label = N_("Max Accel XY [mm/s2]");

public:
    MI_MAX_ACCELERATION_XY();

    /// Shows the current value from the config store
    void reload();

protected:
    void OnClick() override;
};

class MI_MAX_FEEDRATE_XY : public WiSpin {
    static constexpr const char *label = N_("Max Speed XY [mm/s]");

public:
    MI_MAX_FEEDRATE_XY();

    /// Shows the current value from the config store
    void reload();

protected:
    void OnClick() override;
};

class MI_MOTION_LIMITS_DEFAULTS : public IWindowMenuItem {
    static constexpr const char *label = N_("Restore Defaults");

public:
    MI_MOTION_LIMITS_DEFAULTS();

protected:
    void click(IWindowMenu &window_menu) override;
};

using ScreenMenuMotionLimitsSpec = ScreenMenu<EFooter::On,
    MI_RETURN,
    MI_MAX_ACCELERATION_XY,
    MI_MAX_FEEDRATE_XY,
    MI_MOTION_LIMITS_DEFAULTS>;

/// Persistent overrides of the default max acceleration and speed.
/// G-codes (from the slicer, for example) can still lower or raise them until the next reset.
class ScreenMenuMotionLimits : public ScreenMenuMotionLimitsSpec {
public:
    constexpr static const char *label = N_("MOTION LIMITS");
    ScreenMenuMotionLimits();

protected:
    void windowEvent(window_t *sender, GUI_event_t event, void *param) override;
};
