#!/usr/bin/env python3
"""Pack the verified original tirepattern.tga into a texture-only WPK2.

Usage: python3 pack_tire_track.py /path/to/data/sprite/tirepattern.tga /new/path/tiretrack.wpk
The output is created exclusively and must be added at the game-data ZIP root.
Original game data is not distributed by this script.
"""
import hashlib
from pathlib import Path
import struct
import sys

from pack import encode16, load_image_file

SOURCE_SHA256 = '0d5b300e3d417fe6c5abc33c0e2099bea7f9ef0409396dc73f9bd76052d6809f'
OUTPUT_SHA256 = '4b764d1f043c574286561937953c944d9af7cfedfd4f989a47dad4e03da49e01'


def make(source):
    path = Path(source)
    if hashlib.sha256(path.read_bytes()).hexdigest() != SOURCE_SHA256:
        raise ValueError('input is not the verified original tirepattern.tga')
    w, h, fmt, pixels = encode16(load_image_file(str(path)), 256)
    if (w, h, fmt, len(pixels)) != (64, 64, 1, 8192):
        raise ValueError('unexpected tire-pattern dimensions or format')
    header = struct.pack('<4sIIIfIII', b'WPK2', 1, 0, 0, 4000.0, 32, 96, 96)
    table = struct.pack('<48sHHIII', b'tirepattern.tga', w, h, fmt, 96, len(pixels))
    result = header + table + pixels
    if hashlib.sha256(result).hexdigest() != OUTPUT_SHA256:
        raise ValueError('encoded texture does not match the reviewed asset')
    return result


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    data = make(sys.argv[1])
    with open(sys.argv[2], 'xb') as output:
        output.write(data)
    print('wrote verified tire-track WPK2:', OUTPUT_SHA256)


if __name__ == '__main__':
    main()
