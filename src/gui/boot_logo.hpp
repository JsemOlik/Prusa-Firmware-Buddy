/// @file
/// Custom logo shown on the splash screen instead of the stock one
#pragma once

#include <Rect16.h>
#include <guiconfig/GuiDefaults.hpp>

namespace boot_logo {

/// Path of the logo on the USB drive, as produced by utils/boot_logo.py
inline constexpr const char *usb_path = "/usb/boot_logo.qoi";

/// The logo has to fit above the splash screen progress bar
inline constexpr uint16_t max_width = GuiDefaults::ScreenWidth;
inline constexpr uint16_t max_height = GuiDefaults::ScreenHeight / 2;

enum class ImportResult {
    ok,
    no_file,
    invalid,
    write_failed,
};

/// Copies the logo from the USB drive to the internal flash
ImportResult import_from_usb();

/// Deletes the stored logo, the stock one is shown again
void remove();

bool exists();

/// Clears \p area and draws the stored logo centered in it.
/// Does nothing if there is no logo or it does not fit.
void draw(Rect16 area);

} // namespace boot_logo
