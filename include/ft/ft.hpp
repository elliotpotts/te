#ifndef FT_FT_HPP_INCLUDED
#define FT_FT_HPP_INCLUDED

#include <ft2build.h>
#include FT_FREETYPE_H
#include <te/unique.hpp>
#include <string>
#include <ft/face.hpp>

namespace ft {
    std::string error_string(int code);

    struct ft_deleter {
        void operator()(FT_Library);
    };
    using unique_ft = te::unique<FT_Library, ft_deleter>;

    class ft {
        unique_ft hnd;
        static unique_ft make_hnd();
    public:
        ft();
        face make_face(const char* filename, int pts, double aspect);
    };
}

#endif
