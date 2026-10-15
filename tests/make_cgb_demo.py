#!/usr/bin/env python3
"""Generate original CGB homebrew plus an independent reference image.

Uses only the Python standard library. All artwork, font glyphs and CPU program
are original to Game Boy Studio. No commercial game or boot ROM is included.
The omitted Nintendo logo means the demo expects an emulator post-boot entry.
"""
from pathlib import Path
import argparse

ROOT = Path(__file__).resolve().parent.parent

# Original compact 4x5 font. Each hexadecimal digit is one glyph row.
FONT = {
    'A': '699F9', 'B': 'E9E9E', 'C': '78887', 'D': 'E999E',
    'E': 'F8E8F', 'F': 'F8E88', 'G': '78B97', 'H': '99F99',
    'I': 'F666F', 'J': '11196', 'K': '9AC A9'.replace(' ', ''),
    'L': '8888F', 'M': '9FF99', 'N': '9DB99', 'O': '69996',
    'P': 'E9E88', 'Q': '699B7', 'R': 'E9EA9', 'S': '7861E',
    'T': 'F6666', 'U': '99996', 'V': '99966', 'W': '99FF9',
    'X': '99699', 'Y': '99666', 'Z': 'F124F', '0': '69B96',
    '1': '2666F', '2': 'E124F', '3': 'E161E', '4': '99F11',
    '5': 'F8E1E', '6': '68E96', '7': 'F1244', '8': '69696',
    '9': '69716', ':': '06060', '/': '11248', '-': '00F00',
    ' ': '00000', '!': '66606', '+': '06F60',
}


def rgb15(r, g, b):
    return (r // 8) | ((g // 8) << 5) | ((b // 8) << 10)


PALETTES = [
    [rgb15(8, 16, 40), rgb15(32, 64, 96), rgb15(64, 224, 232), rgb15(240, 248, 248)],
    [rgb15(8, 16, 40), rgb15(112, 64, 40), rgb15(248, 176, 64), rgb15(248, 232, 168)],
    [rgb15(8, 16, 40), rgb15(40, 80, 72), rgb15(80, 208, 120), rgb15(176, 248, 144)],
    [rgb15(8, 16, 40), rgb15(104, 40, 88), rgb15(240, 88, 144), rgb15(248, 176, 208)],
    [rgb15(8, 16, 40), rgb15(40, 48, 112), rgb15(96, 120, 248), rgb15(176, 208, 248)],
    [rgb15(8, 16, 40), rgb15(80, 40, 112), rgb15(176, 96, 240), rgb15(232, 192, 248)],
    [rgb15(8, 16, 40), rgb15(88, 80, 32), rgb15(232, 224, 72), rgb15(248, 248, 184)],
    [rgb15(8, 16, 40), rgb15(24, 80, 96), rgb15(56, 184, 240), rgb15(160, 232, 248)],
]
OBJ_PALETTES = [[0, rgb15(248, 248, 248), rgb15(56, 224, 232), rgb15(176, 248, 144)]] + PALETTES[1:]


def palette_bytes(palettes):
    return bytes(value for palette in palettes for color in palette for value in (color & 255, color >> 8))


def to_rgb(color):
    return tuple((v << 3) | (v >> 2) for v in ((color >> shift) & 31 for shift in (0, 5, 10)))


def tile_bytes(rows):
    result = bytearray()
    for row in rows:
        lo = hi = 0
        for x, value in enumerate(row):
            lo |= (value & 1) << (7 - x)
            hi |= ((value >> 1) & 1) << (7 - x)
        result.extend((lo, hi))
    return bytes(result)


class Asm:
    def __init__(self, origin=0x150):
        self.origin, self.mem, self.labels, self.fixups = origin, bytearray(), {}, []

    def here(self):
        return self.origin + len(self.mem)

    def emit(self, *values):
        self.mem.extend(values)

    def label(self, name):
        self.labels[name] = self.here()

    def word(self, value):
        self.emit(value & 255, value >> 8)

    def ld16(self, opcode, value):
        self.emit(opcode)
        self.word(value)

    def symbol(self, opcode, name):
        self.emit(opcode)
        self.fixups.append((len(self.mem), name, False))
        self.word(0)

    def jr(self, opcode, name):
        self.emit(opcode)
        self.fixups.append((len(self.mem), name, True))
        self.emit(0)

    def resolve(self, symbols):
        self.labels.update(symbols)
        for position, name, relative in self.fixups:
            target = self.labels[name]
            if relative:
                target -= self.origin + position + 1
                assert -128 <= target <= 127, (name, target)
                self.mem[position] = target & 255
            else:
                self.mem[position:position + 2] = bytes((target & 255, target >> 8))


def build():
    image = [[0] * 160 for _ in range(144)]
    attrs = bytearray(1024)

    def text(message, x, y, ink=3, scale=1):
        for character in message:
            for row, mask in enumerate(FONT[character]):
                for column in range(4):
                    if int(mask, 16) & (8 >> column):
                        for dy in range(scale):
                            for dx in range(scale):
                                image[y + row * scale + dy][x + column * scale + dx] = ink
            x += 5 * scale

    def line(y, ink=1):
        for x in range(8, 152):
            image[y][x] = ink

    text('GAME BOY', 40, 8, 3, 2)
    text('STUDIO COLOR', 21, 23, 2, 2)
    line(39, 1)
    text('8 PALETTES / 2 VRAM BANKS', 20, 42, 2)

    # Every swatch has asymmetric corner marks. Tile attributes exercise both
    # banks, all eight palettes and all four X/Y flip combinations.
    for ty in range(7, 10):
        for tx in range(2, 18):
            palette = (tx - 2) // 2
            attribute = palette | (8 if (tx + ty) % 2 else 0)
            attribute |= 0x20 if tx & 1 else 0
            attribute |= 0x40 if ty == 8 else 0
            attrs[ty * 32 + tx] = attribute
            for y in range(8):
                for x in range(8):
                    raw = 1 + (y // 3) % 3
                    if x in (0, 7) or y in (0, 7):
                        raw = 0
                    if x == 2 and y in (2, 3):
                        raw = 3
                    image[ty * 8 + y][tx * 8 + x] = raw
    line(85)
    text('CGB NATIVE', 55, 89, 2)
    for y in range(98, 122):
        image[y][15] = image[y][144] = 1
    for x in range(15, 145):
        image[98][x] = image[121][x] = 1
    text('ARROWS: MOVE', 50, 127, 3)
    text('ORIGINAL HOMEBREW', 38, 137, 2)

    tiles = [[], []]
    lookup = [{}, {}]
    tilemap = bytearray(1024)
    for ty in range(18):
        for tx in range(20):
            rows = [image[ty * 8 + y][tx * 8:tx * 8 + 8] for y in range(8)]
            bank = (attrs[ty * 32 + tx] >> 3) & 1
            blob = tile_bytes(rows)
            if blob not in lookup[bank]:
                lookup[bank][blob] = len(tiles[bank])
                tiles[bank].append(blob)
            tilemap[ty * 32 + tx] = lookup[bank][blob]
    assert all(len(bank) < 255 for bank in tiles), tuple(map(len, tiles))
    # Sprite 255 in bank 1: an original tiny spaceship, colour 0 transparent.
    sprite_rows = [[int(c) for c in row] for row in (
        '00022000', '00211200', '02111120', '21133112',
        '22211222', '00033000', '00033000', '00000000')]
    bank0 = b''.join(tiles[0])
    bank1 = b''.join(tiles[1]).ljust(255 * 16, b'\0') + tile_bytes(sprite_rows)

    code = Asm()
    code.emit(0xF3)  # DI
    code.ld16(0x31, 0xFFFE)
    code.label('startup_vblank')
    code.emit(0xF0, 0x44, 0xFE, 144)
    code.jr(0x38, 'startup_vblank')
    code.emit(0xAF, 0xE0, 0x40)  # LCD off before VRAM/OAM writes.

    def copy(source, target, size):
        code.symbol(0x21, source)
        code.ld16(0x11, target)
        code.ld16(0x01, size)
        code.label('copy_' + source)
        code.emit(0x2A, 0x12, 0x13, 0x0B, 0x78, 0xB1)
        code.jr(0x20, 'copy_' + source)

    def palettes(source, index_port, data_port):
        code.emit(0x3E, 0x80, 0xE0, index_port)
        code.symbol(0x21, source)
        code.emit(0x06, 64)
        code.label('copy_' + source)
        code.emit(0x2A, 0xE0, data_port, 0x05)
        code.jr(0x20, 'copy_' + source)

    palettes('bg_palettes', 0x68, 0x69)
    palettes('obj_palettes', 0x6A, 0x6B)
    code.emit(0xAF, 0xE0, 0x4F)
    copy('bank0', 0x8000, len(bank0))
    copy('tilemap', 0x9800, len(tilemap))
    code.emit(0x3E, 1, 0xE0, 0x4F)
    copy('bank1', 0x8000, len(bank1))
    copy('attributes', 0x9800, len(attrs))
    code.emit(0xAF, 0xE0, 0x4F, 0xE0, 0x42, 0xE0, 0x43)
    for address, value in ((0xFE00, 121), (0xFE01, 84), (0xFE02, 255), (0xFE03, 8)):
        code.ld16(0x21, address)
        code.emit(0x36, value)
    code.emit(0x3E, 0x93, 0xE0, 0x40)
    code.label('wait_visible')
    code.emit(0xF0, 0x44, 0xFE, 144)
    code.jr(0x30, 'wait_visible')
    code.label('wait_vblank')
    code.emit(0xF0, 0x44, 0xFE, 144)
    code.jr(0x38, 'wait_vblank')
    code.emit(0x3E, 0x20, 0xE0, 0)
    for _ in range(4):
        code.emit(0xF0, 0)  # Allow the selected JOYP lines to settle.
    code.emit(0x47)
    for bit, address, opcode in ((0, 0xFE01, 0x34), (1, 0xFE01, 0x35),
                                 (2, 0xFE00, 0x35), (3, 0xFE00, 0x34)):
        code.emit(0xCB, 0x40 + 8 * bit)
        code.jr(0x20, 'skip_' + str(bit))
        code.ld16(0x21, address)
        code.emit(opcode)
        code.label('skip_' + str(bit))
    code.symbol(0xC3, 'wait_visible')

    rom = bytearray(32768)
    rom[0x100:0x104] = bytes((0, 0xC3, 0x50, 1))
    title = b'STUDIOCOLOR'  # Fits the 11-byte native-CGB title field.
    rom[0x134:0x134 + len(title)] = title
    rom[0x143] = 0xC0  # Native CGB required.
    rom[0x144:0x146] = b'00'
    rom[0x14A], rom[0x14B] = 1, 0x33
    blobs = {'bank0': bank0, 'bank1': bank1, 'tilemap': tilemap,
             'attributes': attrs, 'bg_palettes': palette_bytes(PALETTES),
             'obj_palettes': palette_bytes(OBJ_PALETTES)}
    cursor, symbols = 0x400, {}
    assert code.here() <= cursor
    for name, blob in blobs.items():
        symbols[name] = cursor
        rom[cursor:cursor + len(blob)] = blob
        cursor += len(blob)
    assert cursor <= len(rom)
    code.resolve(symbols)
    rom[0x150:code.here()] = code.mem
    checksum = 0
    for value in rom[0x134:0x14D]:
        checksum = (checksum - value - 1) & 255
    rom[0x14D] = checksum
    global_sum = sum(rom) & 0xFFFF
    rom[0x14E:0x150] = bytes((global_sum >> 8, global_sum & 255))

    # Independent CPU/PPU-free renderer for a deterministic reference frame.
    rgb = bytearray()
    for y in range(144):
        for x in range(160):
            attr = attrs[(y // 8) * 32 + x // 8]
            ix, iy = x % 8, y % 8
            if attr & 0x20:
                ix = 7 - ix
            if attr & 0x40:
                iy = 7 - iy
            raw = image[(y // 8) * 8 + iy][(x // 8) * 8 + ix]
            color = PALETTES[attr & 7][raw]
            if 76 <= x < 84 and 105 <= y < 113:
                obj_raw = sprite_rows[y - 105][x - 76]
                if obj_raw:
                    color = OBJ_PALETTES[0][obj_raw]
            rgb.extend(to_rgb(color))
    reference = b'P6\n160 144\n255\n' + rgb
    return bytes(rom), reference, tuple(map(len, tiles)), len(code.mem)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'demo.gbc')
    parser.add_argument('--reference', type=Path, default=ROOT / 'tests/cgb-demo-reference.ppm')
    args = parser.parse_args()
    rom, reference, counts, program_size = build()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.reference.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(rom)
    args.reference.write_bytes(reference)
    print(f'Wrote {args.output}: tiles per bank {counts}, program {program_size} bytes')
    print(f'Wrote independent colour reference {args.reference}')


if __name__ == '__main__':
    main()
