import displayio
import time
import math
import terminalio
from adafruit_matrixportal.matrix import Matrix
from adafruit_display_text import bitmap_label

WIDTH, HEIGHT = 64, 32
CX, CY = 31, 15
RADIUS = 13

# bit_depth=4 gives 16 PWM levels for smooth fade gradient
matrix = Matrix(width=WIDTH, height=HEIGHT, bit_depth=4)
display = matrix.display
display.rotation = 180

# G/B swapped panel: blue hex = green on display
palette = displayio.Palette(7)
palette[0] = 0x000000  # black
palette[1] = 0x000018  # very dim (fading out)
palette[2] = 0x000040  # dim (circle)
palette[3] = 0x000070  # medium dim
palette[4] = 0x0000A0  # medium
palette[5] = 0x0000D0  # bright
palette[6] = 0x0000FF  # full (sweep tip)

bitmap = displayio.Bitmap(WIDTH, HEIGHT, 7)
group = displayio.Group()
group.append(displayio.TileGrid(bitmap, pixel_shader=palette))
display.root_group = group

# Precompute circle pixels
circle = set()
for deg in range(360):
    a = math.radians(deg)
    x = int(CX + RADIUS * math.cos(a))
    y = int(CY + RADIUS * math.sin(a))
    if 0 <= x < WIDTH and 0 <= y < HEIGHT:
        circle.add((x, y))

for (x, y) in circle:
    bitmap[x, y] = 2
bitmap[CX, CY] = 3

SPEED = 4     # degrees per frame
FADE_EVERY = 2
MAX_V = 6

active = {}   # (x, y) -> brightness — only active trail pixels tracked

def sweep_line(angle_deg):
    a = math.radians(angle_deg)
    px = []
    for r in range(1, RADIUS + 1):
        x = int(CX + r * math.cos(a))
        y = int(CY + r * math.sin(a))
        if 0 <= x < WIDTH and 0 <= y < HEIGHT:
            px.append((x, y))
    return px

# --- Phase 1: Radar sweep (2 rotations) ---
total_frames = int(360 / SPEED * 2)

for frame in range(total_frames):
    # Decay trail — only iterates active pixels (~100-150 max)
    if frame % FADE_EVERY == 0:
        to_remove = []
        for key in list(active):
            x, y = key
            v = active[key] - 1
            if v <= 0:
                to_remove.append(key)
                bitmap[x, y] = 2 if key in circle else 0
            else:
                active[key] = v
                bitmap[x, y] = v
        for key in to_remove:
            del active[key]

    # Draw new sweep line
    px = sweep_line(frame * SPEED)
    n = len(px)
    for i, (x, y) in enumerate(px):
        v = MAX_V if i >= n - 4 else MAX_V - 1
        active[(x, y)] = v
        bitmap[x, y] = v

    bitmap[CX, CY] = 3
    display.refresh()

# --- Phase 2: Flash ---
for _ in range(4):
    for (x, y) in circle:
        bitmap[x, y] = 6
    display.refresh()
    time.sleep(0.05)
    for (x, y) in circle:
        bitmap[x, y] = 0
    display.refresh()
    time.sleep(0.05)

# --- Phase 3: OVERHEAD TRACKER ---
text_group = displayio.Group()
label1 = bitmap_label.Label(
    terminalio.FONT,
    text="OVERHEAD",
    color=0x0000FF,   # G/B swapped: appears green (matches radar)
    anchor_point=(0.5, 0.5),
    anchored_position=(32, 10),
)
label2 = bitmap_label.Label(
    terminalio.FONT,
    text="TRACKER",
    color=0x008000,   # G/B swapped: appears subtle blue
    anchor_point=(0.5, 0.5),
    anchored_position=(32, 22),
)
text_group.append(label1)
text_group.append(label2)
display.root_group = text_group
display.refresh()
time.sleep(2)

print("Boot complete")
while True:
    time.sleep(1)
