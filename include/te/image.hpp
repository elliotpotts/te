#ifndef TE_IMAGE_HPP_INCLUDED
#define TE_IMAGE_HPP_INCLUDED

#include <FreeImage.h>
#include <string>
#include <memory>

namespace te {
    struct freeimage_bitmap_deleter {
        void operator()(FIBITMAP* bmp) const {
            FreeImage_Unload(bmp);
        }
    };
    using unique_bitmap = std::unique_ptr<FIBITMAP, freeimage_bitmap_deleter>;

    struct freeimage_memory_deleter {
        void operator()(FIMEMORY* bmp) const {
            FreeImage_CloseMemory(bmp);
        }
    };
    using unique_memory = std::unique_ptr<FIMEMORY, freeimage_memory_deleter>;

    unique_bitmap make_bitmap(std::string filename);
    unique_bitmap make_bitmap(const unsigned char* begin, const unsigned char* end);
}
#endif
