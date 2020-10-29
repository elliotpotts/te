#include <ft/ft.hpp>
#include <stdexcept>
#include <spdlog/spdlog.h>
#include <fmt/format.h>

std::string ft::error_string(int code) {
#undef FTERRORS_H_
#define FT_ERROR_START_LIST     switch (code) {
#define FT_ERRORDEF( e, v, s )      case e: return fmt::format(#e ": {}", s);
#define FT_ERROR_END_LIST       };
#include FT_ERRORS_H
    return fmt::format("Unknown FT error: {}", code);
}

void ft::ft_deleter::operator()(FT_Library p) {
    if (int err = FT_Done_FreeType(p)) {
        spdlog::error("Failed to gracefully destroy FT_Library: {}", error_string(err));
    }
}

ft::unique_ft ft::ft::make_hnd() {
    FT_Library lib;
    if (int err = FT_Init_FreeType(&lib)) {
        std::runtime_error(error_string(err));
    }
    return unique_ft { lib };
}

ft::ft::ft() : hnd { make_hnd() } {
}

ft::face ft::ft::make_face(const char* filename, int pts) {
    FT_Face created;
    if (int err = FT_New_Face(*hnd, filename, 0, &created)) {
        throw std::runtime_error(error_string(err));
    }
    // pixel_size = point_size * resolution / 72 
    if (int err = FT_Set_Char_Size (
            created,
            pts * 64, // width
            pts * 64, // height in 1/64th of points
            165, //horizontal DPI
            165) //vertical DPI
        ) {
        throw std::runtime_error(error_string(err));
    }
    return face { face_hnd {created} };
}
