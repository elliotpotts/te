#ifndef HB_FONT_HPP_INCLUDED
#define HB_FONT_HPP_INCLUDED

#include <memory>
#include <hb-ft.h>

namespace hb {
    //TODO: replace with retain_ptr or similar
    struct font_deleter {
        void operator()(hb_font_t* buf);
    };
    using unique_font = std::unique_ptr<hb_font_t, font_deleter>;

    struct font {
        unique_font hnd;
    public:
        font(unique_font&& fnt);
    };
}

#endif
