#ifndef FT_FACE_HPP_INCLUDED
#define FT_FACE_HPP_INCLUDED

#include <ft2build.h>
#include FT_FREETYPE_H
#include <te/unique.hpp>
#include <hb/font.hpp>

namespace ft {
    struct face_deleter {
        void operator()(FT_Face f);
    };
    using face_hnd = te::unique<FT_Face, face_deleter>;

    struct glyph_index {
        FT_UInt ix;
        bool operator==(const glyph_index& rhs) const = default;
    };

    struct face {
        face_hnd hnd;
    public:
        face(face_hnd&& hnd);
        FT_FaceRec* operator->();
        glyph_index operator[](char32_t uc);
        FT_GlyphSlotRec operator[](glyph_index ix);
        operator hb::font() const;
    };
}

namespace std {
    template<>
    struct hash<ft::glyph_index> {
        std::size_t operator()(const ft::glyph_index& x) const noexcept {
            return x.ix;
        }
    };
}

#endif
