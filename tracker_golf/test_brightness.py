import os, board, time, displayio, terminalio, busio, math
from adafruit_matrixportal.matrix import Matrix
from adafruit_display_text import bitmap_label
from digitalio import DigitalInOut
import adafruit_esp32spi.adafruit_esp32spi as esp32spi
import adafruit_requests
from adafruit_connection_manager import get_radio_socketpool, get_radio_ssl_context

WIDTH, HEIGHT = 64, 32
CX, CY, R = 13, 15, 11  # radar on left side

matrix = Matrix(width=WIDTH, height=HEIGHT, bit_depth=4)
display = matrix.display
display.rotation = 180

# Palette: 0=black 1=very dim 2=circle 3=trail 4=bright 5=tip
palette = displayio.Palette(6)
palette[0] = 0x000000
palette[1] = 0x001800
palette[2] = 0x003800
palette[3] = 0x007000
palette[4] = 0x00B000
palette[5] = 0x00FF00

bitmap = displayio.Bitmap(WIDTH, HEIGHT, 6)
group = displayio.Group()
group.append(displayio.TileGrid(bitmap, pixel_shader=palette))

time_label = bitmap_label.Label(terminalio.FONT, text="--:--",
    color=0xFFFFFF, anchor_point=(0.5, 0.5), anchored_position=(48, 11))
date_label = bitmap_label.Label(terminalio.FONT, text="--/--",
    color=0x444444, anchor_point=(0.5, 0.5), anchored_position=(48, 22))
group.append(time_label)
group.append(date_label)
display.root_group = group

# Draw static radar circle
circle = set()
for deg in range(360):
    a = math.radians(deg)
    x = int(CX + R * math.cos(a))
    y = int(CY + R * math.sin(a))
    if 0 <= x < WIDTH and 0 <= y < HEIGHT:
        circle.add((x, y))
for (x, y) in circle:
    bitmap[x, y] = 2
bitmap[CX, CY] = 2

def sweep_pixels(angle_deg):
    a = math.radians(angle_deg)
    return [(int(CX + r * math.cos(a)), int(CY + r * math.sin(a)))
            for r in range(1, R + 1)
            if 0 <= int(CX + r * math.cos(a)) < WIDTH and 0 <= int(CY + r * math.sin(a)) < HEIGHT]

# WiFi
time_label.text = "WIFI"
date_label.text = "..."
display.refresh()

SSID = os.getenv("CIRCUITPY_WIFI_SSID")
PASSWORD = os.getenv("CIRCUITPY_WIFI_PASSWORD")
esp32_cs = DigitalInOut(board.ESP_CS)
esp32_ready = DigitalInOut(board.ESP_BUSY)
esp32_reset = DigitalInOut(board.ESP_RESET)
spi = busio.SPI(board.SCK, board.MOSI, board.MISO)
esp = esp32spi.ESP_SPIcontrol(spi, esp32_cs, esp32_ready, esp32_reset)

while not esp.is_connected:
    try:
        esp.connect_AP(SSID, PASSWORD)
    except RuntimeError:
        time.sleep(1)

pool = get_radio_socketpool(esp)
ssl_context = get_radio_ssl_context(esp)
requests = adafruit_requests.Session(pool, ssl_context)

time_label.text = "--:--"
date_label.text = "--/--"

# Main loop
active = {}
angle = 0
SPEED = 5
last_fetch = -60  # fetch immediately

while True:
    # Fetch time every 60s
    if time.monotonic() - last_fetch >= 60:
        try:
            r = requests.get("https://worldtimeapi.org/api/timezone/Australia/Sydney")
            data = r.json()
            r.close()
            dt = data["datetime"]
            time_label.text = dt[11:16]
            date_label.text = dt[8:10] + "/" + dt[5:7]
            last_fetch = time.monotonic()
        except Exception as e:
            print("Error:", e)

    # Decay trail
    for key in list(active):
        x, y = key
        v = active[key] - 1
        if v <= 0:
            del active[key]
            bitmap[x, y] = 2 if key in circle else 0
        else:
            active[key] = v
            bitmap[x, y] = v

    # New sweep line
    px = sweep_pixels(angle)
    for i, (x, y) in enumerate(px):
        v = 5 if i >= len(px) - 3 else 4
        active[(x, y)] = v
        bitmap[x, y] = v

    bitmap[CX, CY] = 2
    angle = (angle + SPEED) % 360
    display.refresh()
    time.sleep(0.04)
