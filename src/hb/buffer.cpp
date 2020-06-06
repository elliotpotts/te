#include <hb/buffer.hpp>

void hb::buffer_deleter::operator()(hb_buffer_t* buf) {
    hb_buffer_destroy(buf);
}

hb::buffer hb::buffer::shape(ft::face& face, std::string_view text) {
    hb::buffer buf {
        .hnd = unique_buffer { hb_buffer_create() }
    };
    hb_buffer_add_utf8(buf.hnd.get(), text.data(), text.length(), 0, text.length());
    hb_buffer_guess_segment_properties(buf.hnd.get());
    hb::font font = face;
    hb_shape(font.hnd.get(), buf.hnd.get(), nullptr, 0);
    return buf;
}
