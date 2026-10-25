"""Generate the touch button textures used by resources/touch_default.cfg.

GMod content has no vgui/touch/* art (nillerusr ships it only in its Android
APK), so the buttons get simple labelled circles. Output: VTF 7.2, RGBA8888,
64x64, one mip, no low-res image -- the format CTouchControls packs into its
atlas (game/client/touch.cpp, power-of-two square RGBA8888 only).

usage: python tools/make_touch_textures.py <outdir> [--preview <dir>]
"""
import argparse
import math
from pathlib import Path
import struct
import zlib

SIZE = 64
BUTTONS = {
    'gmp_jump': 'JUMP', 'gmp_duck': 'DUCK', 'gmp_fire': 'FIRE', 'gmp_fire2': 'ALT', 'gmp_run': 'RUN', 'gmp_use': 'USE',
    'gmp_spawnmenu': 'Q', 'gmp_contextmenu': 'C', 'gmp_menu': 'MENU',
    'gmp_prev': '<', 'gmp_next': '>',
    'gmp_settings': '@gear', 'gmp_done': 'OK',
    'gmp_noclip': 'FLY', 'gmp_light': 'LAMP',
}
FONT = {  # 5x7 glyphs, one string per row
    '<': ['00010', '00100', '01000', '10000', '01000', '00100', '00010'],
    '>': ['01000', '00100', '00010', '00001', '00010', '00100', '01000'],
    'A': ['01110', '10001', '10001', '11111', '10001', '10001', '10001'],
    'C': ['01110', '10001', '10000', '10000', '10000', '10001', '01110'],
    'D': ['11110', '10001', '10001', '10001', '10001', '10001', '11110'],
    'E': ['11111', '10000', '10000', '11110', '10000', '10000', '11111'],
    'F': ['11111', '10000', '10000', '11110', '10000', '10000', '10000'],
    'I': ['01110', '00100', '00100', '00100', '00100', '00100', '01110'],
    'J': ['00111', '00010', '00010', '00010', '00010', '10010', '01100'],
    'K': ['10001', '10010', '10100', '11000', '10100', '10010', '10001'],
    'L': ['10000', '10000', '10000', '10000', '10000', '10000', '11111'],
    'M': ['10001', '11011', '10101', '10101', '10001', '10001', '10001'],
    'N': ['10001', '11001', '10101', '10011', '10001', '10001', '10001'],
    'O': ['01110', '10001', '10001', '10001', '10001', '10001', '01110'],
    'P': ['11110', '10001', '10001', '11110', '10000', '10000', '10000'],
    'Q': ['01110', '10001', '10001', '10001', '10101', '10010', '01101'],
    'R': ['11110', '10001', '10001', '11110', '10100', '10010', '10001'],
    'S': ['01111', '10000', '10000', '01110', '00001', '00001', '11110'],
    'T': ['11111', '00100', '00100', '00100', '00100', '00100', '00100'],
    'Y': ['10001', '10001', '01010', '00100', '00100', '00100', '00100'],
    'U': ['10001', '10001', '10001', '10001', '10001', '10001', '01110'],
}
# TEXTUREFLAGS_CLAMPS | CLAMPT | NOMIP | NOLOD | EIGHTBITALPHA
FLAGS = 0x4 | 0x8 | 0x100 | 0x200 | 0x2000
IMAGE_FORMAT_RGBA8888 = 0
IMAGE_FORMAT_UNKNOWN = -1


def render(label):
    px = bytearray(SIZE * SIZE * 4)
    c = (SIZE - 1) / 2
    for y in range(SIZE):
        for x in range(SIZE):
            d = math.hypot(x - c, y - c)
            i = (y * SIZE + x) * 4
            if d <= 29.5:
                px[i:i + 4] = bytes((20, 20, 20, 120))
            if 27.5 <= d <= 30.5:
                px[i:i + 4] = bytes((255, 255, 255, 230))
    if label == '@gear':
        for y in range(SIZE):
            for x in range(SIZE):
                dx, dy = x - c, y - c
                d = math.hypot(dx, dy)
                angle = (math.atan2(dy, dx) + math.pi / 8) % (math.pi / 4) - math.pi / 8
                if 8 <= d <= (22 if abs(angle) < .19 else 17):
                    i = (y * SIZE + x) * 4
                    px[i:i + 4] = bytes((255,255,255,255))
        return bytes(px)
    scale = 3 if len(label) <= 2 else 2
    width = (len(label) * 6 - 1) * scale
    x0, y0 = (SIZE - width) // 2, (SIZE - 7 * scale) // 2
    for n, ch in enumerate(label):
        for gy, row in enumerate(FONT[ch]):
            for gx, bit in enumerate(row):
                if bit != '1':
                    continue
                for sy in range(scale):
                    for sx in range(scale):
                        x = x0 + (n * 6 + gx) * scale + sx
                        y = y0 + gy * scale + sy
                        i = (y * SIZE + x) * 4
                        px[i:i + 4] = bytes((255, 255, 255, 255))
    return bytes(px)


def vtf(rgba):
    header = struct.pack('<4s3I2HI2H4x3f4xfiBiBBH', b'VTF\0', 7, 2, 80, SIZE, SIZE, FLAGS, 1, 0,
                         0.5, 0.5, 0.5, 1.0, IMAGE_FORMAT_RGBA8888, 1, IMAGE_FORMAT_UNKNOWN, 0, 0, 1)
    return header.ljust(80, b'\0') + rgba


def png(rgba):
    raw = b''.join(b'\0' + rgba[y * SIZE * 4:(y + 1) * SIZE * 4] for y in range(SIZE))
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>2I5B', SIZE, SIZE, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('outdir', type=Path)
    parser.add_argument('--preview', type=Path)
    args = parser.parse_args()
    args.outdir.mkdir(parents=True, exist_ok=True)
    for name, label in BUTTONS.items():
        rgba = render(label)
        (args.outdir / f'{name}.vtf').write_bytes(vtf(rgba))
        if args.preview:
            args.preview.mkdir(parents=True, exist_ok=True)
            (args.preview / f'{name}.png').write_bytes(png(rgba))


if __name__ == '__main__':
    main()
