#!/usr/bin/env python3
import PIL.Image
import struct
from pathlib import Path
import itertools
import io
import numpy as np
import subprocess
import os

def take(n, iterable):
    "Return first n items of the iterable as a list"
    return list(itertools.islice(iterable, n))

def uint_dtype(num_bytes):
    if num_bytes == 1:
        return np.uint8
    elif num_bytes == 2:
        return np.uint16
    else:
        raise RuntimeError(f"Cannot handle Bpp = {self.format.Bpp}")

class PixFormatRGB565:
    def __init__(self):
        self.name = "RGB565"
        self.Bpp = 2

    def to_rgba(self, rgb565):
        rgba = np.copy(rgb565).repeat(4)
        rgba.shape = rgb565.shape + (4,)

        r = np.around((np.right_shift(rgba[:,:,0], 11) & 0b011111)  / (0b011111) * 255.0)
        g = np.around((np.right_shift(rgba[:,:,1],  5) & 0b111111)  / (0b111111) * 255.0)
        b = np.around((np.right_shift(rgba[:,:,2],  0) & 0b011111)  / (0b011111) * 255.0)
        a = rgba[:,:,3]
        a[:] = (1 - (r == 255) * (g == 0) * (b == 255)) * 255

        return np.dstack((r,g,b,a)).astype(np.uint8)

class PixFormatRGB555:
    def __init__(self):
        self.name = "RGB555"
        self.Bpp = 2

    def to_rgba(self, rgb555):
        rgba = np.copy(rgb555).repeat(4)
        rgba.shape = rgb555.shape + (4,)

        r = np.around((np.right_shift(rgba[:,:,0], 11) & 0b11111)  / (0b11111) * 255.0)
        g = np.around((np.right_shift(rgba[:,:,1],  5) & 0b11111)  / (0b11111) * 255.0)
        b = np.around((np.right_shift(rgba[:,:,2],  0) & 0b11111)  / (0b11111) * 255.0)
        a = rgba[:,:,3]
        a[:] = (1 - (r == 255) * (g == 0) * (b == 255)) * 255

        return np.dstack((r,g,b,a)).astype(np.uint8)

class PixFormatARGB4444:
    def __init__(self):
        self.name = "ARGB4444"
        self.Bpp = 2

    def to_rgba(self, argb4444):
        rgba = np.copy(argb4444).repeat(4)
        rgba.shape = argb4444.shape + (4,)

        a = np.around((np.right_shift(rgba[:,:,0], 12) & 0b1111)  / (0b1111) * 255.0)
        r = np.around((np.right_shift(rgba[:,:,1],  8) & 0b1111)  / (0b1111) * 255.0)
        g = np.around((np.right_shift(rgba[:,:,2],  4) & 0b1111)  / (0b1111) * 255.0)
        b = np.around((np.right_shift(rgba[:,:,3],  0) & 0b1111)  / (0b1111) * 255.0)

        return np.dstack((r,g,b,a)).astype(np.uint8)

class PixFormatIndexed:
    def __init__(self, name, palette):
        self.name = name
        self.Bpp = 1
        self.palette = palette

    def to_rgba(self, ixd):
        #     #TODO: stop hardcoding palette pixformat
        return self.palette[PIXFORMAT_RGB565].to_rgba()[ixd]

PIXFORMAT_RGB565 = PixFormatRGB565()
PIXFORMAT_RGB555 = PixFormatRGB555()
PIXFORMAT_ARGB4444 = PixFormatARGB4444()

class Image:
    def __init__(self, format, pixels):
        self.format = format
        self.pixels = pixels
        self.height = pixels.shape[0]
        self.width = pixels.shape[1]

    def to_rgba(self):
        return self.format.to_rgba(self.pixels)

PALETTE_TAG = b'\x61\x70\x30\x31'
IMAGE_TAG = b'\x62\x67\x36\x61'
TEXT_TAG = b'\x5D\xE4\x07\x00'
#TEXTS_MAGC = b'\x5D\xE4\x07\x00\x3D\xE6\x07\x00'
#IMAGES_MAGIC = b'\xB4\x77\x83\x03\x54\x79\x83\x03'
#ANIMS_MAGIC = b'\x79\xE8\x70\x00\x79\xE9\x70\x00'

ARCHIVE_MAGIC = b'\x75\xb1\xc0\xba\x00\x00\x01\x00'

class Archive:
    def __init__(self):
        self.images = []
        self.palettes = []
        self.anims = []
        self.texts = []
        self.unknown = []

    def parse(infile, fname):
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
            buf = infile.read(entries * pixformat.Bpp)
            pixels = np.frombuffer(buf, dtype=uint_dtype(pixformat.Bpp))
            pixels.shape = (entries, 1)
            return Image(pixformat, pixels)

        def parse_palettes_entry():
            entries, count = struct.unpack("<LL", infile.read(4 * 2))
            palette = {}
            for i in range(count):
                image = parse_palette(entries)
                palette[image.format] = image
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
            buf = infile.read(width  * height * pixformat.Bpp)
            pixels = np.frombuffer(buf, dtype=uint_dtype(pixformat.Bpp))
            pixels.shape = (height, width)
            image = Image(pixformat, pixels)
            ar.images.append(image)

        def parse_sfx_entry():
            # TODO: parse wav data
            infile.seek(0x9EA6DC, os.SEEK_SET)
            def parse_one():
                # sample: 'A....emit.......'
                unknown = infile.read(16)

        def parse_music_entry():
            # TODO: parse mp3 data
            infile.seek(0x1351607, os.SEEK_SET)
            def parse_one():
                unknown1 = infile.read(3)
                infile.read(4)
                infile.read(1)
                print(infile.read(7))
                # always 0
                infile.read(1)
            i = 0
            while i < 85:
                parse_one()
                i += 1

        def parse_texts_entry():
            #magic = b'\x3D\xE6\x07\x00'
            #if magic != infile.read(4):
            #    raise RuntimeError("bad text magic")
            infile.read(8)
            while True and infile.tell() < 0x4976C:
                text = ""
                c = infile.read(1)
                while c != bytes([0]):
                    text += str(c, 'latin-1')
                    c = infile.read(1)
                ar.texts.append(text)
            l = infile.read(16)
            while len(l) > 0:
                print(l)
                l = infile.read(16)

        def parse_X_entry():
            print(infile.read(4))
            print(infile.read(4))
            print(infile.read(4))
            print(infile.read(1))
            print(f"Rows start at {infile.tell():x}")
            rest = infile.read()
            rows = rest.split(b"g6a")
            #for i, row in enumerate(rows):
            #    print(f"Row {i:3<}: {row}")
            print(f"{len(rows):x} rows")
            #rows = 0
            #while infile.readable():
            #    row = infile.read(16)
            #    if len(row) < 16:
            #        print(f"Last row was {len(row)} bytes")
            #        break
            #    rows = rows + 1
            #    print(row)
            #print(f"{rows:x} rows")

        entry_parsers = {
            PALETTE_TAG: parse_palettes_entry,
            IMAGE_TAG: parse_image_entry,
            TEXT_TAG: parse_texts_entry,
            b"\x01\x00\x00\x00": parse_X_entry
        }

        def parse_entry():
            pos = infile.tell()
            type_tag = infile.read(4)
            if type_tag in entry_parsers:
                print(f"Reading entry at {pos:x} (type tag {type_tag})")
                entry_parsers[type_tag]()
            else:
                print(f"Unnown tag: {type_tag} at pos {pos:x}")
                ar.unknown.append((pos, type_tag))
                return False
            return True

        #------------------ Parsing starts
        magic = infile.read(8)
        if (magic != ARCHIVE_MAGIC):
            print(f"invalid magic number")

        if fname == "musi.{}":
            parse_music_entry()
            return ar
        if fname == "text.{}":
            parse_texts_entry()
            return ar

        # last_entry_address: the offset relative to the start of the file of the last entry
        #       total_length: the total number of bytes in the whole file
        last_entry_address, total_length = struct.unpack("<LL", infile.read(8))
        print(f"Starting at {infile.tell():x}")
        #todo: why don't text things have an entry of their own?...
        if last_entry_address == b'\x5D\xE4\x07\x00':
            # at 16
            start = infile.tell()
            i = 0
            while True:
                s = ""
                if infile.peek(4)[0:4] == b"\x01\x00\x00\x00":
                    break
                while infile.peek(1)[0:1] != b'\0':
                    s += str(infile.read(1)[0:1], "latin-1")
                infile.read(1)
                tot_len = infile.tell() - start
                #print(f"@{tot_len:<10} String {i:4>}: {s}")
                i += 1
            # we're at 300909
            # after sep4 we're at 300913
            #chunks = inf.read(2048)
            #for chunk in chunks.split(b"P"):
            #    pass
            #    #print(chunk)
        #TODO: skip until we find a tag we recognise
        while parse_entry():
            pass
        print(f"Finished at {infile.tell():x}")
        return ar
