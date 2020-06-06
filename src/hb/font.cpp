#include <hb/font.hpp>

void hb::font_deleter::operator()(hb_font_t* font) {
    hb_font_destroy(font);
}

hb::font::font(hb::unique_font&& fnt) : hnd(std::move(fnt)) {
}
