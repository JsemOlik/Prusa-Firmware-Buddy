/// @file
#include "screen_menu_user_interface.hpp"

#include <img_resources.hpp>

ScreenMenuUserInterface::ScreenMenuUserInterface()
    : ScreenMenuUserInterface__ {
        _("USER INTERFACE"),
        &img::settings_16x16,
    } {
    EnableLongHoldScreenAction();
}

void ScreenMenuUserInterface::windowEvent(window_t *sender, GUI_event_t event, void *param) {
    if (event == GUI_event_t::LOOP) {
        Item<MI_CUSTOM_THEME_RED>().update_visibility();
        Item<MI_CUSTOM_THEME_GREEN>().update_visibility();
        Item<MI_CUSTOM_THEME_BLUE>().update_visibility();
    }
    ScreenMenuUserInterface__::windowEvent(sender, event, param);
}
