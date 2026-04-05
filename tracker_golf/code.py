import os
import time
import board
import busio
import displayio
import terminalio
from digitalio import DigitalInOut
from adafruit_matrixportal.matrix import Matrix
from adafruit_display_text import bitmap_label
from adafruit_bitmap_font import bitmap_font
import adafruit_esp32spi.adafruit_esp32spi as esp32spi
import adafruit_requests
from adafruit_connection_manager import get_radio_socketpool, get_radio_ssl_context

# ── Hardware ───────────────────────────────────────────────────
matrix = Matrix(width=64, height=32, bit_depth=4)
display = matrix.display
display.rotation = 180


# G/B channels swapped on this panel:
#   0x0000FF → green,  0x00FF00 → blue,  0xFF0000 → red
AMBER      = 0xFF00A0
YELLOW     = 0x700070
CYAN       = 0x005050
GREEN      = 0x000050
WHITE      = 0xFFFFFF
DEEP_BLUE  = 0x004000
LIGHT_BLUE = 0x008060

# ── Location ───────────────────────────────────────────────────
HOME_LAT     = float(os.getenv("HOME_LAT",     "-33.8688"))
HOME_LON     = float(os.getenv("HOME_LON",     "151.2093"))
GEOFENCE_KM  = int(os.getenv("GEOFENCE_KM",   "20"))
ALT_FLOOR_FT = int(os.getenv("ALT_FLOOR_FT",  "200"))

# ── WiFi ───────────────────────────────────────────────────────
SSID     = os.getenv("CIRCUITPY_WIFI_SSID")
PASSWORD = os.getenv("CIRCUITPY_WIFI_PASSWORD")

esp32_cs    = DigitalInOut(board.ESP_CS)
esp32_ready = DigitalInOut(board.ESP_BUSY)
esp32_reset = DigitalInOut(board.ESP_RESET)
spi = busio.SPI(board.SCK, board.MOSI, board.MISO)
esp = esp32spi.ESP_SPIcontrol(spi, esp32_cs, esp32_ready, esp32_reset)

while not esp.is_connected:
    try:
        esp.connect_AP(SSID, PASSWORD)
    except RuntimeError:
        time.sleep(1)

pool        = get_radio_socketpool(esp)
ssl_context = get_radio_ssl_context(esp)
requests    = adafruit_requests.Session(pool, ssl_context)

# ── Static flight data ─────────────────────────────────────────
CHAR_W    = 6
GAP       = 1
STEP      = CHAR_W + GAP
ALT_BAR_H = 27
SPD_BAR_H = 24

tiny_font = bitmap_font.load_font("fonts/tom-thumb.bdf")

def make_bar(x, height, color):
    palette = displayio.Palette(1)
    palette[0] = color
    bitmap = displayio.Bitmap(1, height, 1)
    return displayio.TileGrid(bitmap, pixel_shader=palette, x=x, y=32 - height)

_fail_count = 0

def fetch_flight():
    global _fail_count
    r = None
    try:
        url = f"https://api.overheadtracker.com/flights?lat={HOME_LAT}&lon={HOME_LON}&radius={GEOFENCE_KM}&limit=50"
        r = requests.get(url, timeout=10)
        data = r.json()
        ac = data.get("ac", [])
        _fail_count = 0
        for aircraft in ac:
            alt_raw = aircraft.get("alt_baro") or 0
            alt = int(alt_raw) if isinstance(alt_raw, (int, float)) else 0
            if alt >= ALT_FLOOR_FT:
                callsign = (aircraft.get("flight") or "------").strip()
                route    = aircraft.get("route") or ""
                return callsign, route
        return "------", ""
    except Exception as e:
        print("Fetch error:", e)
        _fail_count += 1
        if _fail_count >= 3:
            print("Resetting ESP32 after", _fail_count, "failures...")
            _fail_count = 0
            try:
                esp.reset()
                while not esp.is_connected:
                    try:
                        esp.connect_AP(SSID, PASSWORD)
                    except RuntimeError:
                        time.sleep(1)
            except Exception as e2:
                print("Reset error:", e2)
        return "------", ""
    finally:
        if r is not None:
            try:
                r.close()
            except Exception:
                pass

# ── Build UI once ─────────────────────────────────────────────
MAX_CHARS = 8  # pre-allocate for longest expected callsign

ui = displayio.Group()
ui.append(make_bar(0,  ALT_BAR_H, DEEP_BLUE))
ui.append(make_bar(63, SPD_BAR_H, LIGHT_BLUE))

# Pre-allocate callsign character labels (4 layers each for thick text)
char_labels = []
for i in range(MAX_CHARS):
    row = []
    for dx, dy in ((0, 0), (1, 0), (0, 1), (1, 1)):
        lbl = bitmap_label.Label(
            terminalio.FONT, text=" ", color=AMBER,
            anchor_point=(0.5, 0.5),
            anchored_position=(0, 6 + dy),
        )
        ui.append(lbl)
        row.append(lbl)
    char_labels.append(row)

route_label_top = bitmap_label.Label(
    tiny_font, text="", color=WHITE,
    anchor_point=(0.5, 0.5), anchored_position=(32, 19),
)
route_label_bot = bitmap_label.Label(
    tiny_font, text="", color=WHITE,
    anchor_point=(0.5, 0.5), anchored_position=(32, 27),
)
ui.append(route_label_top)
ui.append(route_label_bot)

# Progress bar: 64×1 bitmap at bottom row, fills left→right over REFRESH_S
prog_palette = displayio.Palette(2)
prog_palette[0] = 0x000000
prog_palette[1] = AMBER
prog_bitmap = displayio.Bitmap(64, 1, 2)
ui.append(displayio.TileGrid(prog_bitmap, pixel_shader=prog_palette, x=0, y=31))

display.root_group = ui

def update_callsign(callsign):
    n       = len(callsign)
    total_w = n * CHAR_W + (n - 1) * GAP
    start_x = (64 - total_w) // 2
    for i in range(MAX_CHARS):
        ch = callsign[i] if i < n else " "
        cx = start_x + i * STEP + CHAR_W // 2 if i < n else 0
        for j, (dx, _) in enumerate(((0, 0), (1, 0), (0, 1), (1, 1))):
            char_labels[i][j].text = ch
            char_labels[i][j].anchored_position = (cx + dx, char_labels[i][j].anchored_position[1])

def update_progress(pixel):
    for x in range(64):
        prog_bitmap[x, 0] = 1 if x < pixel else 0

# ── Main loop ──────────────────────────────────────────────────
REFRESH_S      = 5
PIXEL_INTERVAL = REFRESH_S / 64
last_fetch     = 0
current_pixel  = 0
last_pixel_t   = 0

while True:
    now = time.monotonic()

    if now - last_fetch >= REFRESH_S:
        last_fetch    = time.monotonic()
        current_pixel = 0
        last_pixel_t  = last_fetch
        callsign, route = fetch_flight()
        update_callsign(callsign)
        parts = route.split(" > ", 1) if route else []
        route_label_top.text = parts[0] if len(parts) > 0 else ""
        route_label_bot.text = parts[1] if len(parts) > 1 else ""
        update_progress(0)
        print("Showing:", callsign, route)

    if current_pixel < 64 and now - last_pixel_t >= PIXEL_INTERVAL:
        current_pixel += 1
        last_pixel_t  += PIXEL_INTERVAL
        update_progress(current_pixel)

    display.refresh()
    time.sleep(0.05)
