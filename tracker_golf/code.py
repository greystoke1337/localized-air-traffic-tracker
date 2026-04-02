import board
import displayio
import time
import math
import terminalio
from adafruit_matrixportal.matrix import Matrix
from adafruit_display_text import bitmap_label
from adafruit_seesaw.seesaw import Seesaw
from adafruit_seesaw.neopixel import NeoPixel as SeesawNeoPixel

# ── Hardware ───────────────────────────────────────────────────
matrix = Matrix(width=64, height=32, bit_depth=4)
display = matrix.display
display.rotation = 180

i2c = board.STEMMA_I2C()
seesaw = Seesaw(i2c, addr=0x36)
enc_pixel = SeesawNeoPixel(seesaw, 6, 1)
enc_pixel.fill(0)
brightness = 0.5
display.framebuffer.brightness = brightness
last_enc_pos = seesaw.encoder_position(0)

# G/B channels swapped on this panel:
#   0x0000FF → green,  0x00FF00 → blue,  0xFF0000 → red
# Yellow on display = R + G = R_hex + B_hex = 0xFF00xx
# G/B swap: amber on display = R_hex + B_hex (B_hex drives the G channel)
AMBER = 0xFF00A0

# ── Boot: radar sweep ──────────────────────────────────────────
W, H = 64, 32
CX, CY, R = 31, 15, 13

radar_pal = displayio.Palette(7)
for i, c in enumerate([0x000000, 0x000018, 0x000040, 0x000070, 0x0000A0, 0x0000D0, 0x0000FF]):
    radar_pal[i] = c

bmp = displayio.Bitmap(W, H, 7)
g = displayio.Group()
g.append(displayio.TileGrid(bmp, pixel_shader=radar_pal))
display.root_group = g

circle = set()
for deg in range(360):
    a = math.radians(deg)
    x, y = int(CX + R * math.cos(a)), int(CY + R * math.sin(a))
    if 0 <= x < W and 0 <= y < H:
        circle.add((x, y))
for x, y in circle:
    bmp[x, y] = 2
bmp[CX, CY] = 3

def sweep(angle_deg):
    a = math.radians(angle_deg)
    pts = []
    for r in range(1, R + 1):
        x, y = int(CX + r * math.cos(a)), int(CY + r * math.sin(a))
        if 0 <= x < W and 0 <= y < H:
            pts.append((x, y))
    return pts

active = {}
for frame in range(int(360 / 4 * 2)):
    if frame % 2 == 0:
        for key in list(active):
            x, y = key
            v = active[key] - 1
            if v <= 0:
                del active[key]
                bmp[x, y] = 2 if key in circle else 0
            else:
                active[key] = v
                bmp[x, y] = v
    pts = sweep(frame * 4)
    n = len(pts)
    for i, (x, y) in enumerate(pts):
        v = 6 if i >= n - 4 else 5
        active[(x, y)] = v
        bmp[x, y] = v
    bmp[CX, CY] = 3
    display.refresh()

for _ in range(4):
    for x, y in circle:
        bmp[x, y] = 6
    display.refresh()
    time.sleep(0.05)
    for x, y in circle:
        bmp[x, y] = 0
    display.refresh()
    time.sleep(0.05)

# Title
title = displayio.Group()
title.append(bitmap_label.Label(terminalio.FONT, text="OVERHEAD",
    color=AMBER, anchor_point=(0.5, 0.5), anchored_position=(32, 10)))
title.append(bitmap_label.Label(terminalio.FONT, text="TRACKER",
    color=0x000040, anchor_point=(0.5, 0.5), anchored_position=(32, 22)))
display.root_group = title
display.refresh()
time.sleep(2)

# ── Fake flight ────────────────────────────────────────────────
CALLSIGN = "QFA421"

# ── Flight display — thick callsign with 1px letter gap ────────
# Each character is rendered as 4 overlapping labels (+1px x/y) for
# pseudo-bold. Characters are placed individually so we can add a
# 1px gap between them (bitmap_label has no letter-spacing option).
CHAR_W = 6   # terminalio.FONT character width in px
GAP    = 1   # gap between characters in px
STEP   = CHAR_W + GAP

n       = len(CALLSIGN)
total_w = n * CHAR_W + (n - 1) * GAP
start_x = (64 - total_w) // 2   # left edge of first character

ui = displayio.Group()
for i, ch in enumerate(CALLSIGN):
    cx = start_x + i * STEP + CHAR_W // 2   # center x of this char
    for dx, dy in ((0, 0), (1, 0), (0, 1), (1, 1)):
        ui.append(bitmap_label.Label(
            terminalio.FONT, text=ch, color=AMBER,
            anchor_point=(0.5, 0.5),
            anchored_position=(cx + dx, 16 + dy),
        ))
display.root_group = ui
display.refresh()

print("Showing:", CALLSIGN)

# ── Encoder brightness ─────────────────────────────────────────
while True:
    pos = seesaw.encoder_position(0)
    if pos != last_enc_pos:
        brightness = max(0.05, min(1.0, brightness + (pos - last_enc_pos) * 0.05))
        display.framebuffer.brightness = brightness
        last_enc_pos = pos
    time.sleep(0.05)
