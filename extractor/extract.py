#!/usr/bin/env python3
import PIL.Image
import struct
from pathlib import Path
import io
import itertools

def take(n, iterable):
    "Return first n items of the iterable as a list"
    return list(itertools.islice(iterable, n))

ARCHIVE_MAGIC = b'\x75\xb1\xc0\xba\x00\x00\x01\x00'
#IMAGES_MAGIC = b'\xB4\x77\x83\x03\x54\x79\x83\x03'
#ANIMS_MAGIC = b'\x79\xE8\x70\x00\x79\xE9\x70\x00'
#TEXTS_MAGC = b'\x5D\xE4\x07\x00\x3D\xE6\x07\x00'

PALETTE_TAG = b'\x61\x70\x30\x31'
IMAGE_TAG = b'\x62\x67\x36\x61'

class PixFormatRGB565:
    def __init__(self):
        self.name = "RGB565"
        self.Bpp = 2

    def rgba(self, pixel_bytes):
        pixel = int.from_bytes(pixel_bytes, byteorder="little")
        r = round(((pixel >> 11) & 0b11111)  / (0b11111) * 255.0)
        g = round(((pixel >> 5)  & 0b111111) / (0b111111) * 255.0)
        b = round(((pixel >> 0)  & 0b11111)  / (0b11111) * 255.0)
        if (r, g, b) == (255, 0, 255):
            a = 0
        else:
            a = 255
        return (r,g,b,a)

class PixFormatRGB555:
    def __init__(self):
        self.name = "RGB555"
        self.Bpp = 2

    def rgba(self, pixel_bytes):
        pixel = int.from_bytes(pixel_bytes, byteorder="little")
        r = round(((pixel >> 10) & 0b11111) / 0b11111 * 255.0)
        g = round(((pixel >> 5 ) & 0b11111) / 0b11111 * 255.0)
        b = round(((pixel >> 0 ) & 0b11111) / 0b11111 * 255.0)
        if (r, g, b) == (255, 0, 255):
            a = 0
        else:
            a = 255
        return (r,g,b,a)

class PixFormatARGB4444:
    def __init__(self):
        self.name = "ARGB4444"
        self.Bpp = 2

    def rgba(self, pixel_bytes):
        pixel = int.from_bytes(pixel_bytes, byteorder="little")
        a = round(((pixel >> 12) & 0b1111) / 0b1111 * 255.0)
        r = round(((pixel >> 8 ) & 0b1111) / 0b1111 * 255.0)
        g = round(((pixel >> 4 ) & 0b1111) / 0b1111 * 255.0)
        b = round(((pixel >> 0 ) & 0b1111) / 0b1111 * 255.0)
        return (r,g,b,a)

class PixFormatIndexed:
    def __init__(self, name, palette):
        self.name = name
        self.Bpp = 1
        self.palette = palette

    def rgba(self, pixel_bytes):
        colour_ix = int.from_bytes(pixel_bytes, byteorder="little")
        #TODO: stop hardcoding this
        return self.palette[PIXFORMAT_RGB565].rgba(colour_ix, 0)

PIXFORMAT_RGB565 = PixFormatRGB565()
PIXFORMAT_RGB555 = PixFormatRGB555()
PIXFORMAT_ARGB4444 = PixFormatARGB4444()

class Image:
    def __init__(self, format, width, height, bytes):
        self.format = format
        self.width = width
        self.height = height
        self.raw = bytes

    def rgba(self, x, y):
        linear_ix = y * (self.width * self.format.Bpp) + (x * self.format.Bpp)
        pixel = self.raw[linear_ix : linear_ix + self.format.Bpp]
        return self.format.rgba(pixel)

class Archive:
    def __init__(self):
        self.images = []
        self.palettes = []
        self.anims = []
        self.texts = []

    def parse(infile):
        ar = Archive()

        def parse_pixel_format():
            format = infile.read(4)
            if format == b"5550":
                return PIXFORMAT_RGB555
            elif format == b"5650":
                return PIXFORMAT_RGB565
            elif format == b"4444":
                return PIXFORMAT_ARGB4444
            else:
                return PixFormatIndexed(f"Palette#{len(ar.palettes) - 1}", ar.palettes[-1])

        def parse_palette(entries):
            pixformat = parse_pixel_format()
            #TODO: what is unknown?
            unknown = infile.read(4)
            pixels = infile.read(entries * pixformat.Bpp)
            return Image(pixformat, entries, 1, pixels)

        def parse_palettes_entry():
            entries, count = struct.unpack("<LL", infile.read(4 * 2))
            palette = {}
            for i in range(count):
                image = parse_palette(entries)
                palette[image.format] = image
                print(f"Palette {len(ar.palettes)}.{image.format.name}")
            ar.palettes.append(palette)

        def parse_image_entry():
            #TODO: what is unknown1?
            unknown1 = infile.read(4)
            pixformat = parse_pixel_format()
            #TODO: what is unknown2?
            unknown2 = infile.read(4)
            width, height = struct.unpack("<LL", infile.read(4 * 2))
            #TODO: what is unknown3?
            unknown3 = infile.read(12)
            data = infile.read(width  * height * pixformat.Bpp)
            image = Image(pixformat, width, height, data)
            print(f"Image {len(ar.images)}: {width}x{height}, {pixformat.name}")
            ar.images.append(image)

        entry_parsers = {
            PALETTE_TAG: parse_palettes_entry,
            IMAGE_TAG: parse_image_entry,
        }

        def parse_entry():
            pos = infile.tell()
            type_tag = infile.read(4)
            if type_tag in entry_parsers:
                entry_parsers[type_tag]()
            else:
                print(f"No parser for tag {type_tag.hex(' ')} at {pos}")
                return False
                #TODO: skip until we find a tag we recognise
            return True

        def check_magic():
            magic = infile.read(8)
            if (magic != ARCHIVE_MAGIC):
                print(f"invalid magic number")

        check_magic()
        #todo: figure this out: maybe checksum? length?
        unknown = infile.read(8)
        while parse_entry():
            pass
        return ar

if __name__ == "__main__":
    archive_name = "bldg.{}"
    with open(f"orig/Data/{archive_name}", "rb") as inf:
        ar = Archive.parse(inf)
        outdir = Path(f"processed/{archive_name}")
        outdir.mkdir(parents=True, exist_ok=True)

        for ix, original in take(75, enumerate(ar.images)):
            out_image = PIL.Image.new("RGBA", (original.width, original.height))
            for y in range(0, original.height):
                for x in range(0, original.width):
                    out_image.putpixel((x, y), original.rgba(x, y))
            with open(outdir / f"{ix:04}.png", "wb") as outf:
                out_image.save(outf, "PNG")

    print("Done.")