#ifndef HB_BUFFER_HPP_INCLUDED
#define HB_BUFFER_HPP_INCLUDED

#include <memory>
#include <hb.h>
#include <string_view>
#include <ft/face.hpp>

namespace hb {
    //TODO: replace with retain_ptr or similar
    struct buffer_deleter {
        void operator()(hb_buffer_t* buf);
    };
    using unique_buffer = std::unique_ptr<hb_buffer_t, buffer_deleter>;

    struct buffer {
        unique_buffer hnd;
        static buffer shape(ft::face& face, std::string_view text);
    };
}

#endif
