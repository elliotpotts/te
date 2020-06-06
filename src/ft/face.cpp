#include <ft/ft.hpp>
#include <ft/face.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

void ft::face_deleter::operator()(FT_Face face) {
    if (int err = FT_Done_Face(face)) {
        spdlog::error("Failed to gracefully destroy FT_Face: {}", error_string(err));
    }
}

ft::face::face(face_hnd&& hnd) : hnd { std::move(hnd) } {
}

FT_FaceRec* ft::face::operator->() {
    return *hnd;
}

ft::glyph_index ft::face::operator[](char32_t uc) {
    return glyph_index {FT_Get_Char_Index(*hnd, uc)};
}

FT_GlyphSlotRec ft::face::operator[](glyph_index glyph) {
    if (int err = FT_Load_Glyph (
            *hnd,
            glyph.ix,
            FT_LOAD_DEFAULT
        )) {
        throw std::runtime_error(error_string(err));
    }
    if (int err = FT_Render_Glyph((*hnd)->glyph, FT_RENDER_MODE_NORMAL)) {
        throw std::runtime_error(error_string(err));
    }
    return *((*hnd)->glyph);
}

ft::face::operator hb::font() const {
    return hb::font { hb::unique_font { hb_ft_font_create(*hnd, nullptr) } };
}
