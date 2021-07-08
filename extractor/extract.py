#!/usr/bin/env python3

import archive
import viewer
import argparse
import os
from pathlib import Path
import PIL

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Extract data from `.{}` files')
    parser.add_argument("-x", dest="extract", action="store_true", help="extract files in addition to listing archive contents")
    parser.add_argument("filepath", metavar="filepath", help="path to archive file to list or extract")
    args = parser.parse_args()
    archive_name = os.path.basename(args.filepath)
    print(args.filepath)
    with open(args.filepath, "rb") as inf:
        ar = archive.Archive.parse(inf, archive_name)
        print(f"{len(ar.images):x} images found")
        inf.read()
        print(f"Final tell: {inf.tell():x}")
        print("-----------------------")
        viewer.view(ar)

        # ar.images[0].to_rgba()
        # outdir = Path(f"debug/{archive_name}")
        # outdir.mkdir(parents=True, exist_ok=True)
        # for ix, original in enumerate(ar.images):
        #     out_image = PIL.Image.fromarray(original.to_rgba())
        #     with open(outdir / f"{ix:04}.png", "wb") as outf:
        #         out_image.save(outf, "PNG")
