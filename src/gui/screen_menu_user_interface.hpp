/// @file
#pragma once

#include "MItem_menus.hpp"
#include "MItem_tools.hpp"
#include <MItem_filament.hpp>
#include <basic_screen_menu.hpp>
#include <screen_menu_footer_settings.hpp>

#include <option/has_tool_mapping.h>

#include <option/has_touch.h>
#if HAS_TOUCH()
    #include "MItem_touch.hpp"
#endif

using ScreenMenuUserInterface__ = BasicScreenMenu<
    MI_FOOTER_SETTINGS,
    MI_COLOR_THEME,
    MI_CUSTOM_THEME_RED,
    MI_CUSTOM_THEME_GREEN,
    MI_CUSTOM_THEME_BLUE,
    MI_SOUND_MODE,
    MI_FINISH_MELODY,
    MI_COOLDOWN_AFTER_PRINT,
    MI_SORT_FILES,
    MI_PRINT_PROGRESS_TIME,
    MI_TIMEOUT,
    MI_FILAMENT_CHANGE_PREHEAT_ALL,
#if HAS_TOOL_MAPPING()
    MI_TOOL_MAPPING_PRESET,
#endif
    MI_ASK_COLOR_ON_LOAD,
#if HAS_ST7789_DISPLAY()
    // We could potentionally have MINI display without buzzer.
    // So we only allow sound control for ST7789
    MI_SOUND_VOLUME,
#endif
#if HAS_ILI9488_DISPLAY()
    MI_DISPLAY_BAUDRATE,
#endif
#if HAS_TOUCH()
    MI_ENABLE_TOUCH,
    TOUCH_SIG_WORKAROUND,
#endif
    MI_ALWAYS_HIDDEN>;

class ScreenMenuUserInterface final : public ScreenMenuUserInterface__ {
public:
    ScreenMenuUserInterface();

protected:
    void windowEvent(window_t *sender, GUI_event_t event, void *param) override;
};
