#!/usr/bin/env python3
from PIL import Image
import struct
from pathlib import Path

def process_file(path):
    with open(path) as in_file:
        magic = in_file.read(4)
        if (magic != b'\x75\xb1\xc0\xba\x00\x00\x01\x00'):
            print(f"{path}: invalid magic number")
        file_header = in_file.read(4)
        i = 0
        while True:
            print("@{}".format(in_file.tell()))
            header = in_file.read(18 * 2)


orig = "a_ui,6.{}"
with open("orig/Data/{}".format(orig), "rb") as inf:
    file_header = inf.read(8 * 2)
    i = 0
    while True:
        print("Header at offset {}".format(inf.tell()))
        header = inf.read(18 * 2)
        b0, b1, b2, b3, b4, b5, width, height = struct.unpack (
            "=HHHHHH4xHxxH14x",
            header
        )
        Bpp = 2
        pitch = width * Bpp
        print("{}:{}x{}        {}.{}.{}.{}.{}.{}".format(i, width, height, b0, b1, b2, b3, b4, b5))
        if (b0, b1, b2, b3, b4, b5) == (26466.24886.2.0.13877.12341)
        pixels = inf.read(width * height * 2)
        if len(pixels) != width * height * 2:
            raise RuntimeError("Didn't read all data")
        Path(f"processed/{orig}").mkdir(parents=True, exist_ok=True)
        with open(f"processed/{orig}/{i:03}.png", "wb") as outf:
            img = Image.new("RGBA", (width, height))
            for x in range(0, width):
                for y in range(0, height):
                    pixel_ix = y * pitch + (x * Bpp)
                    pixel, = struct.unpack("<H", pixels[pixel_ix : pixel_ix + Bpp])
                    r = round(((pixel >> 11) & 0b11111)  / (0b11111) * 255.0)
                    g = round(((pixel >> 5)  & 0b111111) / (0b111111) * 255.0)
                    b = round(((pixel >> 0)  & 0b11111)  / (0b11111) * 255.0)
                    if (r, g, b) == (255, 0, 255):
                        a = 0
                    else:
                        a = 255
                    img.putpixel((x,y), (r,g,b,a))
            img.save(outf, "PNG")
            i += 1
    
