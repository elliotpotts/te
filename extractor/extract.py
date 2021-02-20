#!/usr/bin/env python3

import archive
import viewer
import argparse
import os

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Extract data from `.{}` files')
    parser.add_argument("-x", dest="extract", action="store_true", help="extract files in addition to listing archive contents")
    parser.add_argument("filepath", metavar="filepath", help="path to archive file to list or extract")
    args = parser.parse_args()
    archive_name = os.path.basename(args.filepath)
    print(args.filepath)
    with open(args.filepath, "rb") as inf:
        ar = archive.Archive.parse(inf)
        print(f"{len(ar.images):x} images found")
        inf.read()
        print(f"Final tell: {inf.tell():x}")
        print("-----------------------")
        viewer.view(ar)
        #outdir = Path(f"processed/{archive_name}")
        #outdir.mkdir(parents=True, exist_ok=True)
        #for ix, original in enumerate(ar.images):
        #    out_image = PIL.Image.new("RGBA", (original.width, original.height))
        #    for y in range(0, original.height):
        #        for x in range(0, original.width):
        #            out_image.putpixel((x, y), original.rgba(x, y))
        #    with open(outdir / f"{ix:04}.png", "wb") as outf:
        #        out_image.save(outf, "PNG")
