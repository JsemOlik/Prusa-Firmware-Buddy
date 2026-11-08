#pragma once

#include "screen_menu.hpp"
#include <WindowMenuItems.hpp>
#include <WindowMenuSpin.hpp>
#include <marlin_stubs/M1990.hpp>

class MI_SPEED_TEST_ITERATIONS : public WiSpin {
    static constexpr const char *label = N_("Iterations");

public:
    MI_SPEED_TEST_ITERATIONS();
};

class MI_SPEED_TEST_RUNS : public WiSpin {
    static constexpr const char *label = N_("Number of Tests");

public:
    MI_SPEED_TEST_RUNS();
};

class MI_SPEED_TEST_SPEED_BASE : public WiSpin {
public:
    MI_SPEED_TEST_SPEED_BASE(uint8_t index);
};

template <uint8_t index>
class MI_SPEED_TEST_SPEED : public MI_SPEED_TEST_SPEED_BASE {
public:
    MI_SPEED_TEST_SPEED()
        : MI_SPEED_TEST_SPEED_BASE(index) {}
};

class MI_SPEED_TEST_ACCEL_BASE : public WiSpin {
public:
    MI_SPEED_TEST_ACCEL_BASE(uint8_t index);
};

template <uint8_t index>
class MI_SPEED_TEST_ACCEL : public MI_SPEED_TEST_ACCEL_BASE {
public:
    MI_SPEED_TEST_ACCEL()
        : MI_SPEED_TEST_ACCEL_BASE(index) {}
};

class MI_SPEED_TEST_START : public IWindowMenuItem {
    static constexpr const char *label = N_("Start Test");

public:
    MI_SPEED_TEST_START();

protected:
    void click(IWindowMenu &window_menu) override;
};

static_assert(speed_test::max_runs == 3);

using ScreenMenuSpeedTestSpec = ScreenMenu<EFooter::On,
    MI_RETURN,
    MI_SPEED_TEST_ITERATIONS,
    MI_SPEED_TEST_RUNS,
    MI_SPEED_TEST_SPEED<0>,
    MI_SPEED_TEST_ACCEL<0>,
    MI_SPEED_TEST_SPEED<1>,
    MI_SPEED_TEST_ACCEL<1>,
    MI_SPEED_TEST_SPEED<2>,
    MI_SPEED_TEST_ACCEL<2>,
    MI_SPEED_TEST_START>;

class ScreenMenuSpeedTest : public ScreenMenuSpeedTestSpec {
public:
    constexpr static const char *label = N_("SPEED TEST");
    ScreenMenuSpeedTest();

protected:
    void windowEvent(window_t *sender, GUI_event_t event, void *param) override;

private:
    void update_visibility();
    void run_test();
};
