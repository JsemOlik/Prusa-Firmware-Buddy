#include "screen_menu_speed_test.hpp"

#include "ScreenHandler.hpp"
#include "img_resources.hpp"
#include "marlin_client.hpp"
#include "screen_menu_motion_limits.hpp"
#include "window_dlg_wait.hpp"
#include <DialogHandler.hpp>
#include <common/marlin_vars.hpp>
#include <config_store/store_instance.hpp>
#include <window_msgbox.hpp>

#include <algorithm>
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

/// [mm/s^2]
constexpr unsigned search_step = 1000;

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
    case Status::cancelled:
        return N_("Cancelled");
    }
    return "";
}

/// Translates \p text into \p buffer, for use as a printf argument
template <size_t N>
const char *translate(std::array<char, N> &buffer, const char *text) {
    _(text).copyToRAM(buffer.data(), buffer.size());
    return buffer.data();
}

using ProgressText = std::array<char, 128>;

/// @param runs number of tests, 0 for the acceleration search
void format_progress(ProgressText &text, uint8_t runs) {
    using Phase = speed_test::Progress::Phase;
    const auto &progress = speed_test::progress();

    std::array<char, 48> phase;
    switch (progress.phase.load()) {
    case Phase::idle:
    case Phase::homing:
        translate(phase, N_("Homing"));
        break;
    case Phase::measuring_before:
    case Phase::measuring_after:
        translate(phase, N_("Measuring position"));
        break;
    case Phase::moving: {
        std::array<char, 32> format;
        snprintf(phase.data(), phase.size(), translate(format, N_("Iteration %u/%u")), progress.iteration.load(), progress.iterations.load());
        break;
    }
    }

    std::array<char, 40> header;
    if (runs) {
        std::array<char, 32> test;
        snprintf(header.data(), header.size(), "%s %u/%u", translate(test, N_("Test")), progress.slot.load() + 1, runs);
    } else {
        translate(header, N_("Searching"));
    }

    snprintf(text.data(), text.size(), "%s: %u mm/s, %lu mm/s2\n%s",
        header.data(), progress.speed.load(), static_cast<unsigned long>(progress.accel.load()), phase.data());
}

class ProgressBox final : public MsgBoxBase {
public:
    ProgressBox()
        : MsgBoxBase(GuiDefaults::DialogFrameRect, { Response::Abort, Response::_none, Response::_none, Response::_none }, 0, {}) {}

    void set_progress(const string_view_utf8 &str) {
        // window_text_t skips the redraw when handed the same buffer again, even if its contents changed
        text.SetText({});
        text.SetText(str);
    }
};

/// Shows the progress of the queued M1990s until they all finish, offering to cancel them
/// @param runs number of tests, 0 for the acceleration search
void show_progress(uint8_t runs) {
    // Two buffers, because the dialog keeps pointing to the displayed one
    struct {
        ProgressBox box;
        std::array<ProgressText, 2> texts {};
        size_t shown = 0;
        uint8_t runs = 0;
    } state;
    state.runs = runs;

    Screens::Access()->gui_loop_until_dialog_closed([s = &state] {
        // Allows popping up a warning dialog on top of this one
        DialogHandler::Access().Loop();

        auto &next = s->texts[1 - s->shown];
        format_progress(next, s->runs);
        if (strcmp(next.data(), s->texts[s->shown].data())) {
            s->shown = 1 - s->shown;
            s->box.set_progress(string_view_utf8::MakeRAM(next.data()));
        }

        if (!marlin_vars().is_processing.get()) {
            Screens::Access()->close_dialog();
        }
    });

    if (state.box.GetResult() == Response::Abort) {
        // The moves already queued take a moment to finish, and the printer re-homes afterwards
        speed_test::progress().cancel = true;
        window_dlg_wait_t::wait_for_gcodes_to_finish();
    }
}

/// Prepares for queueing M1990s
void start() {
    // Wait for anything still running, so that the results are not read or written concurrently
    window_dlg_wait_t::wait_for_gcodes_to_finish();
    speed_test::progress().cancel = false;
}

using ResultsText = std::array<char, 384>;

size_t format_results(ResultsText &text, const speed_test::RunResults &results) {
    using Status = speed_test::RunResult::Status;

    size_t len = 0;
    for (size_t i = 0; i < results.size() && len < text.size(); i++) {
        const auto &result = results[i];
        if (result.status == Status::not_run) {
            continue;
        }

        std::array<char, 32> status;
        len += snprintf(text.data() + len, text.size() - len, "%u: %u mm/s, %lu mm/s2\n%s",
            static_cast<unsigned>(i + 1), result.speed, static_cast<unsigned long>(result.accel), translate(status, status_label(result.status)));
        if (len >= text.size()) {
            break;
        }

        if (result.status == Status::passed || result.status == Status::skipped_steps) {
            len += snprintf(text.data() + len, text.size() - len, " (X %.2f, Y %.2f mm)\n",
                static_cast<double>(result.drift_x), static_cast<double>(result.drift_y));
        } else {
            len += snprintf(text.data() + len, text.size() - len, "\n");
        }
    }
    return std::min(len, text.size() - 1);
}

size_t format_search(ResultsText &text, const speed_test::SearchResult &search) {
    using Status = speed_test::RunResult::Status;

    std::array<char, 48> label;
    size_t len = snprintf(text.data(), text.size(), "%s: %u mm/s\n", translate(label, N_("Speed")), search.speed);

    const auto append = [&](const char *label_text, uint32_t accel) {
        if (len < text.size()) {
            len += snprintf(text.data() + len, text.size() - len, "%s: %lu mm/s2\n", translate(label, label_text), static_cast<unsigned long>(accel));
        }
    };

    if (search.passed_accel) {
        append(N_("Highest passed"), search.passed_accel);
    }
    if (search.failed_accel) {
        append(N_("Skipped steps at"), search.failed_accel);
    }
    if (search.status == Status::failed || search.status == Status::cancelled) {
        if (len < text.size()) {
            len += snprintf(text.data() + len, text.size() - len, "%s\n", translate(label, status_label(search.status)));
        }
    }
    if (search.recommended_accel()) {
        append(N_("Recommended"), search.recommended_accel());
    }
    return std::min(len, text.size() - 1);
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

MI_SPEED_TEST_ACTION::MI_SPEED_TEST_ACTION(const char *label)
    : IWindowMenuItem(_(label)) {}

void MI_SPEED_TEST_ACTION::click(IWindowMenu & /*window_menu*/) {
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
        if (param == &Item<MI_SPEED_TEST_START>()) {
            run_test();
        } else if (param == &Item<MI_SPEED_TEST_SEARCH>()) {
            run_search();
        } else if (param == &Item<MI_SPEED_TEST_LAST_RESULTS>()) {
            show_last_results();
        }
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

    start();
    auto &results = speed_test::results();
    results.fill({});
    for (uint8_t i = 0; i < runs; i++) {
        marlin_client::gcode_printf("M1990 S%u A%u I%d R%u", combos[i].speed, combos[i].accel, iterations, i);
    }
    show_progress(runs);

    config_store().speed_test_results.set(results);

    ResultsText text;
    format_results(text, results);
    MsgBoxInfo(string_view_utf8::MakeRAM(text.data()), Responses_Ok);
}

void ScreenMenuSpeedTest::run_search() {
    const auto iterations = static_cast<int>(Item<MI_SPEED_TEST_ITERATIONS>().value());
    const auto speed = static_cast<unsigned>(Item<MI_SPEED_TEST_SPEED<0>>().value());
    const auto accel = static_cast<unsigned>(Item<MI_SPEED_TEST_ACCEL<0>>().value());

    start();
    speed_test::results().fill({});
    marlin_client::gcode_printf("M1990 S%u A%u I%d F%u", speed, accel, iterations, search_step);
    show_progress(0);

    const auto search = speed_test::search_result();
    config_store().speed_test_search_result.set(search);

    ResultsText text;
    size_t len = format_search(text, search);
    const uint32_t recommended = search.recommended_accel();
    if (recommended == 0) {
        MsgBoxInfo(string_view_utf8::MakeRAM(text.data()), Responses_Ok);
        return;
    }

    std::array<char, 96> question;
    snprintf(text.data() + len, text.size() - len, "\n%s", translate(question, N_("Use the recommended value as the max acceleration?")));
    if (MsgBoxQuestion(string_view_utf8::MakeRAM(text.data()), Responses_YesNo) == Response::Yes) {
        motion_limits::set_max_acceleration_xy(recommended);
    }
}

void ScreenMenuSpeedTest::show_last_results() {
    const auto results = config_store().speed_test_results.get();
    const auto search = config_store().speed_test_search_result.get();

    const bool has_results = std::ranges::any_of(results, [](const auto &result) { return result.status != speed_test::RunResult::Status::not_run; });
    const bool has_search = search.status != speed_test::RunResult::Status::not_run;
    if (!has_results && !has_search) {
        MsgBoxInfo(_("No results yet"), Responses_Ok);
        return;
    }

    ResultsText text;
    if (has_results) {
        format_results(text, results);
        MsgBoxInfo(string_view_utf8::MakeRAM(text.data()), Responses_Ok);
    }
    if (has_search) {
        format_search(text, search);
        MsgBoxInfo(string_view_utf8::MakeRAM(text.data()), Responses_Ok);
    }
}
