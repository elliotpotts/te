#include <te/image.hpp>
#include <fmt/format.h>

te::unique_bitmap te::make_bitmap(const unsigned char* begin, const unsigned char* end) {
    //TODO: get rid of this const cast
    unique_memory memory_img { FreeImage_OpenMemory(const_cast<unsigned char*>(begin), end - begin) };
    if (!memory_img) {
        memory_img.release();
        throw std::runtime_error("Couldn't open memory!");
    }
    FREE_IMAGE_FORMAT fmt = FreeImage_GetFileTypeFromMemory(memory_img.get());
    if (fmt == FIF_UNKNOWN) {
        throw std::runtime_error("Couldn't determine file format of memory image!");
    }
    unique_bitmap bitmap { FreeImage_LoadFromMemory(fmt, memory_img.get(), 0) };
    if (!bitmap) {
        bitmap.release();
        throw std::runtime_error("Couldn't load memory image.");
    }
    return unique_bitmap { FreeImage_ConvertTo32Bits(bitmap.get()) };
}

te::unique_bitmap te::make_bitmap(std::string filename) {
    FREE_IMAGE_FORMAT fmt = FreeImage_GetFileType(filename.c_str());
    if (fmt == FIF_UNKNOWN) {
        throw std::runtime_error(fmt::format("Couldn't determine image file format from filename: {}", filename));
    }
    unique_bitmap bitmap {FreeImage_Load(fmt, filename.c_str())};
    if (!bitmap) {
        throw std::runtime_error("Couldn't load image.");
    }
    return bitmap;
}
