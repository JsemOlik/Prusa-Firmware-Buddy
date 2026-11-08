#include "boot_logo.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <optional>
#include <display.hpp>
#include <qoi_decoder.hpp>
#include <unique_file_ptr.hpp>

namespace boot_logo {

namespace {

constexpr const char *stored_path = "/system/boot_logo.qoi";
constexpr const char *temporary_path = "/system/boot_logo.tmp";

using Header = std::array<std::byte, qoi::Decoder::HEADER_SIZE>;

struct Size {
    uint16_t width;
    uint16_t height;
};

std::optional<Size> read_size(FILE *file) {
    Header header;
    if (fread(header.data(), 1, header.size(), file) != header.size() || memcmp(header.data(), "qoif", 4) != 0) {
        return std::nullopt;
    }

    const auto width = qoi::Decoder::get_image_width(header);
    const auto height = qoi::Decoder::get_image_height(header);
    if (width == 0 || height == 0 || width > max_width || height > max_height) {
        return std::nullopt;
    }
    return Size { static_cast<uint16_t>(width), static_cast<uint16_t>(height) };
}

bool copy(FILE *source, FILE *destination) {
    std::array<uint8_t, 128> buffer;
    while (const size_t read = fread(buffer.data(), 1, buffer.size(), source)) {
        if (fwrite(buffer.data(), 1, read, destination) != read) {
            return false;
        }
    }
    return !ferror(source);
}

class FileReader final : public AbstractByteReader {
public:
    explicit FileReader(FILE *file)
        : file(file) {}

    WritableBytes read(WritableBytes buffer) final {
        return { buffer.data(), fread(buffer.data(), 1, buffer.size(), file) };
    }

private:
    FILE *file;
};

} // namespace

ImportResult import_from_usb() {
    unique_file_ptr source(fopen(usb_path, "rb"));
    if (!source) {
        return ImportResult::no_file;
    }
    if (!read_size(source.get())) {
        return ImportResult::invalid;
    }
    rewind(source.get());

    {
        unique_file_ptr destination(fopen(temporary_path, "wb"));
        if (!destination || !copy(source.get(), destination.get())) {
            ::remove(temporary_path);
            return ImportResult::write_failed;
        }
    }

    // Replace the old logo only after the new one is completely written
    ::remove(stored_path);
    if (rename(temporary_path, stored_path) != 0) {
        ::remove(temporary_path);
        return ImportResult::write_failed;
    }
    return ImportResult::ok;
}

void remove() {
    ::remove(stored_path);
}

bool exists() {
    return unique_file_ptr(fopen(stored_path, "rb")) != nullptr;
}

void draw(Rect16 area) {
    unique_file_ptr file(fopen(stored_path, "rb"));
    if (!file) {
        return;
    }
    const auto size = read_size(file.get());
    if (!size || size->width > area.Width() || size->height > area.Height()) {
        return;
    }
    rewind(file.get());

    display::fill_rect(area, COLOR_BLACK);
    FileReader reader(file.get());
    display::draw_img(
        point_ui16_t {
            static_cast<uint16_t>(area.Left() + (area.Width() - size->width) / 2),
            static_cast<uint16_t>(area.Top() + (area.Height() - size->height) / 2),
        },
        reader);
}

} // namespace boot_logo
