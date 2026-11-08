#include "screen_menu_speed_test.hpp"

#include "ScreenHandler.hpp"
#include "img_resources.hpp"
#include "marlin_client.hpp"
#include "window_dlg_wait.hpp"
#include <DialogHandler.hpp>
#include <common/marlin_vars.hpp>
#include <window_msgbox.hpp>

#include <cstdio>
#include <cstring>

namespace {

constexpr NumericInputConfig iterations_config {
    .min_value = 1,
    .max_value = speed_test::max_iterations,
};

constexpr NumericInputConfig runs_config {
    .min_value = 1,
    .max_value = speed_test::max_runs,
};

constexpr NumericInputConfig speed_config {
    .min_value = 50,
    .max_value = 500,
    .step = 10,
};

constexpr NumericInputConfig accel_config {
    .min_value = 1000,
    .max_value = 20000,
    .step = 500,
};

struct Combo {
    uint16_t speed;
    uint16_t accel;
};

constexpr std::array<Combo, speed_test::max_runs> default_combos { {
    { 200, 5000 },
    { 300, 10000 },
    { 400, 20000 },
} };

constexpr std::array<const char *, speed_test::max_runs> speed_labels {
    N_("Speed 1 [mm/s]"),
    N_("Speed 2 [mm/s]"),
    N_("Speed 3 [mm/s]"),
};

constexpr std::array<const char *, speed_test::max_runs> accel_labels {
    N_("Accel 1 [mm/s2]"),
    N_("Accel 2 [mm/s2]"),
    N_("Accel 3 [mm/s2]"),
};

const char *status_label(speed_test::RunResult::Status status) {
    using Status = speed_test::RunResult::Status;
    switch (status) {
    case Status::not_run:
        return N_("Not run");
    case Status::passed:
        return N_("Passed");
    case Status::skipped_steps:
        return N_("Skipped steps");
    case Status::failed:
        return N_("Failed");
    }
    return "";
}

using ProgressText = std::array<char, 128>;

void format_progress(ProgressText &text, uint8_t runs) {
    using Phase = speed_test::Progress::Phase;
    const auto &progress = speed_test::progress();

    std::array<char, 48> phase;
    switch (progress.phase.load()) {
    case Phase::idle:
    case Phase::homing:
        _("Homing").copyToRAM(phase.data(), phase.size());
        break;
    case Phase::measuring_before:
    case Phase::measuring_after:
        _("Measuring position").copyToRAM(phase.data(), phase.size());
        break;
    case Phase::moving: {
        std::array<char, 32> format;
        _("Iteration %u/%u").copyToRAM(format.data(), format.size());
        snprintf(phase.data(), phase.size(), format.data(), progress.iteration.load(), progress.iterations.load());
        break;
    }
    }

    std::array<char, 32> test;
    _("Test").copyToRAM(test.data(), test.size());
    snprintf(text.data(), text.size(), "%s %u/%u: %u mm/s, %lu mm/s2\n%s",
        test.data(), progress.slot.load() + 1, runs, progress.speed.load(), static_cast<unsigned long>(progress.accel.load()), phase.data());
}

} // namespace

MI_SPEED_TEST_ITERATIONS::MI_SPEED_TEST_ITERATIONS()
    : WiSpin(1, iterations_config, _(label)) {}

MI_SPEED_TEST_RUNS::MI_SPEED_TEST_RUNS()
    : WiSpin(1, runs_config, _(label)) {}

MI_SPEED_TEST_SPEED_BASE::MI_SPEED_TEST_SPEED_BASE(uint8_t index)
    : WiSpin(default_combos[index].speed, speed_config, _(speed_labels[index])) {}

MI_SPEED_TEST_ACCEL_BASE::MI_SPEED_TEST_ACCEL_BASE(uint8_t index)
    : WiSpin(default_combos[index].accel, accel_config, _(accel_labels[index])) {}

MI_SPEED_TEST_START::MI_SPEED_TEST_START()
    : IWindowMenuItem(_(label)) {}

void MI_SPEED_TEST_START::click(IWindowMenu & /*window_menu*/) {
    Screens::Access()->WindowEvent(GUI_event_t::CHILD_CLICK, this);
}

ScreenMenuSpeedTest::ScreenMenuSpeedTest()
    : ScreenMenuSpeedTestSpec(_(label)) {
    header.SetIcon(&img::calibrate_white_16x16);
    update_visibility();
}

void ScreenMenuSpeedTest::windowEvent(window_t *sender, GUI_event_t event, void *param) {
    switch (event) {
    case GUI_event_t::LOOP:
        update_visibility();
        break;
    case GUI_event_t::CHILD_CLICK:
        run_test();
        return;
    default:
        break;
    }
    ScreenMenu::windowEvent(sender, event, param);
}

void ScreenMenuSpeedTest::update_visibility() {
    const auto runs = static_cast<uint8_t>(Item<MI_SPEED_TEST_RUNS>().value());
    const auto set_hidden = [](IWindowMenuItem &item, bool hidden) {
        if (item.IsHidden() != hidden) {
            item.set_is_hidden(hidden);
        }
    };
    set_hidden(Item<MI_SPEED_TEST_SPEED<1>>(), runs < 2);
    set_hidden(Item<MI_SPEED_TEST_ACCEL<1>>(), runs < 2);
    set_hidden(Item<MI_SPEED_TEST_SPEED<2>>(), runs < 3);
    set_hidden(Item<MI_SPEED_TEST_ACCEL<2>>(), runs < 3);
}

void ScreenMenuSpeedTest::run_test() {
    const auto iterations = static_cast<int>(Item<MI_SPEED_TEST_ITERATIONS>().value());
    const auto runs = static_cast<uint8_t>(Item<MI_SPEED_TEST_RUNS>().value());
    const std::array<Combo, speed_test::max_runs> combos { {
        { static_cast<uint16_t>(Item<MI_SPEED_TEST_SPEED<0>>().value()), static_cast<uint16_t>(Item<MI_SPEED_TEST_ACCEL<0>>().value()) },
        { static_cast<uint16_t>(Item<MI_SPEED_TEST_SPEED<1>>().value()), static_cast<uint16_t>(Item<MI_SPEED_TEST_ACCEL<1>>().value()) },
        { static_cast<uint16_t>(Item<MI_SPEED_TEST_SPEED<2>>().value()), static_cast<uint16_t>(Item<MI_SPEED_TEST_ACCEL<2>>().value()) },
    } };

    // Wait for anything still running, so that the results cleared below are not written concurrently
    window_dlg_wait_t::wait_for_gcodes_to_finish();

    auto &results = speed_test::results();
    results.fill({});
    for (uint8_t i = 0; i < runs; i++) {
        marlin_client::gcode_printf("M1990 S%u A%u I%d R%u", combos[i].speed, combos[i].accel, iterations, i);
    }

    // Two buffers, because the dialog keeps pointing to the displayed one
    struct {
        std::array<ProgressText, 2> texts {};
        size_t shown = 0;
        uint8_t runs;
    } state { .runs = runs };
    window_dlg_wait_t::wait_until({}, [s = &state](window_dlg_wait_t &dlg) {
        // Allows popping up a warning dialog on top of this one
        DialogHandler::Access().Loop();

        auto &next = s->texts[1 - s->shown];
        format_progress(next, s->runs);
        if (strcmp(next.data(), s->texts[s->shown].data())) {
            s->shown = 1 - s->shown;
            dlg.set_text(string_view_utf8::MakeRAM(next.data()));
        }
        return !marlin_vars().is_processing.get();
    });

    std::array<char, 384> text;
    size_t len = 0;
    for (uint8_t i = 0; i < runs && len < text.size(); i++) {
        const auto &result = results[i];

        std::array<char, 32> status;
        _(status_label(result.status)).copyToRAM(status.data(), status.size());

        if (result.status == speed_test::RunResult::Status::passed || result.status == speed_test::RunResult::Status::skipped_steps) {
            len += snprintf(text.data() + len, text.size() - len, "%u: %u mm/s, %lu mm/s2\n%s (X %.2f, Y %.2f mm)\n",
                i + 1, result.speed, static_cast<unsigned long>(result.accel), status.data(),
                static_cast<double>(result.drift_x), static_cast<double>(result.drift_y));
        } else {
            len += snprintf(text.data() + len, text.size() - len, "%u: %u mm/s, %u mm/s2\n%s\n",
                i + 1, combos[i].speed, combos[i].accel, status.data());
        }
    }

    MsgBoxInfo(string_view_utf8::MakeRAM(text.data()), Responses_Ok);
}
