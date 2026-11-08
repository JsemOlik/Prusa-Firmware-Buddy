#!/usr/bin/env python3
"""Converts an image to a boot logo for the splash screen.

Copy the output to the root of a USB drive as boot_logo.qoi and use
Settings > User Interface > Load Boot Logo from USB on the printer.
"""
from argparse import ArgumentParser
from pathlib import Path

import numpy
import qoi
from PIL import Image

# Has to match the limits in src/gui/boot_logo.hpp
MAX_WIDTH = 480
MAX_HEIGHT = 160


def main():
    parser = ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path, help='Image to convert')
    parser.add_argument('output',
                        type=Path,
                        nargs='?',
                        default=Path('boot_logo.qoi'),
                        help='Output file (default: boot_logo.qoi)')
    parser.add_argument('--width',
                        type=int,
                        default=MAX_WIDTH,
                        help=f'Maximal width (default: {MAX_WIDTH})')
    parser.add_argument('--height',
                        type=int,
                        default=MAX_HEIGHT,
                        help=f'Maximal height (default: {MAX_HEIGHT})')
    args = parser.parse_args()

    width = min(args.width, MAX_WIDTH)
    height = min(args.height, MAX_HEIGHT)

    with Image.open(args.input) as image:
        image = image.convert('RGBA')
        image.thumbnail((width, height), Image.LANCZOS)

        # The splash screen is black, flatten the transparency against it
        background = Image.new('RGBA', image.size, (0, 0, 0, 255))
        background.alpha_composite(image)
        logo = background.convert('RGB')

    args.output.write_bytes(qoi.encode(numpy.array(logo)))
    print(f'{args.output}: {logo.width}x{logo.height}')


if __name__ == '__main__':
    main()
