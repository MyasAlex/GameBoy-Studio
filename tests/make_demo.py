"""Generate a freely distributable original DMG test/demo ROM using Pillow.
The generated demo.gb is already included: Pillow is needed only to regenerate it.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent.parent
out = HERE / 'demo.gb'
im = Image.new('L', (160, 144), 0)
d = ImageDraw.Draw(im)
font_path = '/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf'
try:
    title = ImageFont.truetype(font_path, 13)
    small = ImageFont.truetype(font_path, 9)
except OSError:
    title = ImageFont.load_default()
    small = ImageFont.load_default()

d.rectangle((0, 0, 159, 25), fill=3)
d.text((18, 5), 'GB STUDIO BETA', font=title, fill=0)
d.rectangle((0, 26, 159, 28), fill=2)
d.text((20, 35), 'GAME BOY DMG', font=small, fill=3)
d.text((20, 48), 'HOMEBREW TEST ROM', font=small, fill=2)
d.line((5, 60, 154, 60), fill=1)
d.rectangle((8, 66, 151, 119), outline=2, width=2)
d.text((23, 123), 'ARROWS: MOVE BOX', font=small, fill=3)
d.text((24, 135), 'F1 PALETTE  F5 SAVE', font=small, fill=2)

pixels = im.load()
tiles = [bytes(16), bytes([255] * 16)]  # tile 0 blank, tile 1 black (sprite)
lookup = {tiles[0]: 0, tiles[1]: 1}
map20 = []
for y in range(18):
    for x in range(20):
        blob = bytearray()
        for r in range(8):
            lo = hi = 0
            for col in range(8):
                pix = pixels[x*8+col, y*8+r]
                lo |= (pix & 1) << (7-col)
                hi |= ((pix >> 1) & 1) << (7-col)
            blob.extend([lo, hi])
        t = bytes(blob)
        if t not in lookup:
            lookup[t] = len(tiles)
            tiles.append(t)
        map20.append(lookup[t])
assert len(tiles) <= 256, len(tiles)
map32 = bytearray(1024)
for y in range(18):
    for x in range(20):
        map32[y * 32 + x] = map20[y * 20 + x]
tiledata = b''.join(tiles)

class Asm:
    def __init__(self, origin): self.mem = bytearray(); self.origin = origin; self.labels = {}; self.fixups = []
    def here(self): return self.origin+len(self.mem)
    def label(self, name): self.labels[name] = self.here()
    def emit(self, *values): self.mem.extend(values)
    def word(self, value): self.emit(value&255,(value>>8)&255)
    def ld16(self, opcode, value): self.emit(opcode); self.word(value)
    def ldsrc(self, name): self.emit(0x21); self.fixups.append((len(self.mem), name, 'abs')); self.word(0)
    def jr(self, opcode, name): self.emit(opcode); self.fixups.append((len(self.mem),name,'rel')); self.emit(0)
    def jp(self, name): self.emit(0xC3); self.fixups.append((len(self.mem),name,'abs')); self.word(0)
    def resolve(self, symbols):
        self.labels.update(symbols)
        for index,name,kind in self.fixups:
            target=self.labels[name]
            if kind=='abs':self.mem[index:index+2]=bytes((target&255,target>>8))
            else:
                diff=target-(self.origin+index+1)
                assert -128<=diff<=127,(name,diff)
                self.mem[index]=diff&255

code = Asm(0x0150)
code.emit(0xF3)  # DI

def copy(source_label, dst, size):
    code.ldsrc(source_label)
    code.ld16(0x11,dst) # DE
    code.ld16(0x01,size) # BC
    loop=f'copy_{source_label}'
    code.label(loop)
    code.emit(0x2A,0x12,0x13,0x0B,0x78,0xB1) # LD A,(HL+); LD (DE),A; INC DE; DEC BC; LD A,B; OR C
    code.jr(0x20,loop)

copy('tiledata',0x8000,len(tiledata))
copy('tilemap',0x9800,len(map32))
# LCDC = BG + OBJ, BGP = 4 grayscale shades, sprite palette E4
for reg,value in [(0x40,0x93),(0x47,0xE4),(0x48,0xE4)]:
    code.emit(0x3E,value,0xE0,reg)
# Sprite 0: OAM Y=98, X=80, Tile=1, Attribute=0
for address,value in [(0xFE00,96),(0xFE01,80),(0xFE02,1),(0xFE03,0)]:
    code.ld16(0x21,address);code.emit(0x36,value)
code.label('wait_start')
code.emit(0xF0,0x44,0xFE,0x90) # LY<144 (wait for visible scanline)
code.jr(0x30,'wait_start')
code.label('wait_vblank')
code.emit(0xF0,0x44,0xFE,0x90)
code.jr(0x38,'wait_vblank')
code.emit(0x3E,0x20,0xE0,0x00,0xF0,0x00,0x47) # select dpad, read joyp to B
for bit,addr,inc in [(0,0xFE01,0x34),(1,0xFE01,0x35),(2,0xFE00,0x35),(3,0xFE00,0x34)]:
    code.emit(0xCB,0x40+8*bit)
    skip=f'skip{bit}'
    code.jr(0x20,skip)
    code.ld16(0x21,addr)
    code.emit(inc)
    code.label(skip)
code.jp('wait_start')

rom=bytearray(32768)
rom[0x100:0x103]=bytes((0xC3,0x50,0x01)) # jump past header
rom[0x134:0x143]=b'GB STUDIO DEMO\0'
rom[0x143]=0x00
rom[0x147]=0x00
rom[0x148]=0x00
rom[0x149]=0x00
rom[0x14A]=0x01
rom[0x14B]=0x33
source_addr=0x0300
code.resolve({'tiledata':source_addr,'tilemap':source_addr+len(tiledata)})
assert code.here()<=source_addr
assert source_addr+len(tiledata)+len(map32)<=32768
rom[0x150:code.here()]=code.mem
rom[source_addr:source_addr+len(tiledata)]=tiledata
rom[source_addr+len(tiledata):source_addr+len(tiledata)+len(map32)]=map32
# Nintendo logo is deliberately omitted; emulator begins in post-boot state.
checksum = 0
for b in rom[0x134:0x14D]:checksum=(checksum-b-1)&0xFF
rom[0x14D]=checksum
out.write_bytes(rom)
print(f'Wrote {out} ({len(tiles)} unique 8x8 tiles, {len(code.mem)} bytes CPU program)')
