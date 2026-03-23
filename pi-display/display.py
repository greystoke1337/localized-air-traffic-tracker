import os
import sys
import math
import time
import numpy as np
import pygame
import requests

os.environ['SDL_VIDEODRIVER'] = 'offscreen'
os.environ['SDL_NOMOUSE']     = '1'

W, H = 480, 320

# ── Location config ──────────────────────────────────────
HOME_LAT   = -33.8530    # Russell Lea, Sydney
HOME_LON   = 151.1410
RADIUS_KM  = 15

# ── Timing ───────────────────────────────────────────────
REFRESH         = 10     # seconds between data polls
PAGE_ROTATE_SEC = 15     # seconds per page
WEATHER_REFRESH = 300    # weather poll interval (5 min)
FB_DEV          = '/dev/fb1'

# ── Endpoint URLs ────────────────────────────────────────
BASE            = os.environ.get('PROXY_URL', 'https://api.overheadtracker.com')
STATS_URL       = BASE + '/stats'
PEAK_URL        = BASE + '/peak'
_api_radius     = int(math.ceil(RADIUS_KM / 1.852 * 4.0))
FLIGHTS_URL     = f'{BASE}/flights?lat={HOME_LAT}&lon={HOME_LON}&radius={_api_radius}'
WEATHER_URL     = f'{BASE}/weather?lat={HOME_LAT}&lon={HOME_LON}'
STATUS_URL      = BASE + '/status'
DEVICE_ECHO_URL = BASE + '/device/echo/status'
DEVICE_FOX_URL  = BASE + '/device/foxtrot/status'

# ── Colors ───────────────────────────────────────────────
BG     = (15,  15,  25)
ACCENT = (0,  180, 255)
GREEN  = (0,  220, 100)
AMBER  = (255, 180,  0)
RED    = (255,  80,  80)
DIM    = (80,   90, 110)
WHITE  = (230, 235, 245)
ORANGE = (255, 136,  68)
GOLD   = (255, 170,   0)
YELLOW = (255, 255,   0)
ROW_BG = (22,  22,  35)
BLUE   = (68,  136, 255)

PHASE_COLORS = {
    'TAKEOFF':  GREEN,
    'CLIMBING': GREEN,
    'CRUISING': DIM,
    'DESCEND':  ORANGE,
    'APPROACH': GOLD,
    'LANDING':  RED,
    'OVERHEAD': BLUE,
    'UNKNOWN':  DIM,
}

PAGE_COUNT = 3
FETCH_FAILED = 'FETCH_FAILED'

# ── Pygame init ──────────────────────────────────────────
pygame.init()
screen  = pygame.display.set_mode((W, H))
font_lg = pygame.font.SysFont("monospace", 26, bold=True)
font_md = pygame.font.SysFont("monospace", 18)
font_sm = pygame.font.SysFont("monospace", 13)
fb      = open(FB_DEV, 'wb')


# ── Pure logic ───────────────────────────────────────────

def haversine_km(lat1, lon1, lat2, lon2):
    R = 6371.0
    dLat = math.radians(lat2 - lat1)
    dLon = math.radians(lon2 - lon1)
    a = (math.sin(dLat / 2) ** 2 +
         math.cos(math.radians(lat1)) * math.cos(math.radians(lat2)) *
         math.sin(dLon / 2) ** 2)
    return R * 2.0 * math.atan2(math.sqrt(a), math.sqrt(1 - a))


def derive_phase(alt, vs, dist):
    if alt <= 0:
        return 'UNKNOWN'
    if dist < 2.0 and alt < 8000:
        return 'OVERHEAD'
    if alt < 3000:
        if vs < -200: return 'LANDING'
        if vs >  200: return 'TAKEOFF'
        if vs <  -50: return 'APPROACH'
    if vs < -100: return 'DESCEND'
    if vs >  100: return 'CLIMBING'
    return 'CRUISING'


def format_alt(alt):
    if alt <= 0:
        return '---'
    if alt >= 10000:
        return f'FL{alt // 100:03d}'
    return f'{alt} FT'


def process_flights(raw):
    if not raw or 'ac' not in raw:
        return []
    processed = []
    for ac in raw['ac']:
        lat = ac.get('lat')
        lon = ac.get('lon')
        alt = ac.get('alt_baro')
        if not lat or not lon or not alt or alt == 'ground':
            continue
        alt = int(alt)
        dist = haversine_km(HOME_LAT, HOME_LON, float(lat), float(lon))
        if dist > RADIUS_KM:
            continue
        vs = int(ac.get('baro_rate', 0) or 0)
        phase = derive_phase(alt, vs, dist)
        cs = (ac.get('flight') or '').strip()
        processed.append({
            'callsign': cs or '------',
            'alt':      alt,
            'dist':     dist,
            'phase':    phase,
            'route':    ac.get('route', ''),
        })
    processed.sort(key=lambda f: f['dist'])
    return processed[:2]


# ── Flights-today tracking ───────────────────────────────

seen_today     = set()
last_reset_day = time.localtime().tm_yday


def track_flights_today(flights):
    global seen_today, last_reset_day
    today = time.localtime().tm_yday
    if today != last_reset_day:
        seen_today.clear()
        last_reset_day = today
    for f in flights:
        cs = f.get('callsign', '').strip()
        if cs and cs != '------':
            seen_today.add(cs)


# ── Data fetching ────────────────────────────────────────

def fetch_all():
    results = {}
    for name, url in [('stats', STATS_URL), ('peak', PEAK_URL),
                       ('flights', FLIGHTS_URL)]:
        try:
            timeout = 12 if name == 'flights' else 3
            resp = requests.get(url, timeout=timeout)
            if name == 'flights' and resp.status_code != 200:
                results[name] = FETCH_FAILED
                continue
            body = resp.json()
            if name == 'flights' and 'error' in body:
                results[name] = FETCH_FAILED
                continue
            results[name] = body
        except Exception:
            results[name] = FETCH_FAILED if name == 'flights' else None
    return results.get('stats'), results.get('peak'), results.get('flights')


def fetch_weather():
    try:
        return requests.get(WEATHER_URL, timeout=5).json()
    except Exception:
        return None


def fetch_server_status():
    results = {}
    for name, url in [('status', STATUS_URL),
                       ('echo', DEVICE_ECHO_URL),
                       ('foxtrot', DEVICE_FOX_URL)]:
        try:
            results[name] = requests.get(url, timeout=3).json()
        except Exception:
            results[name] = None
    return results.get('status'), results.get('echo'), results.get('foxtrot')


# ── Drawing helpers ──────────────────────────────────────

def flush_to_fb():
    arr = pygame.surfarray.array3d(screen).transpose(1, 0, 2)
    r = arr[:, :, 0].astype(np.uint16)
    g = arr[:, :, 1].astype(np.uint16)
    b = arr[:, :, 2].astype(np.uint16)
    rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    fb.seek(0)
    fb.write(rgb565.astype('<u2').tobytes())
    fb.flush()


def draw_text(text, font, color, x, y, align='left'):
    surf = font.render(text, True, color)
    if align == 'center':
        x = x - surf.get_width() // 2
    elif align == 'right':
        x = x - surf.get_width()
    screen.blit(surf, (x, y))


def draw_title_bar(healthy, current_page):
    pygame.draw.circle(screen, GREEN if healthy else RED, (14, 12), 5)
    draw_text('OVERHEAD TRACKER', font_md, ACCENT, 26, 3)
    for i in range(PAGE_COUNT):
        x = W - 30 + i * 16
        if i == current_page:
            pygame.draw.circle(screen, ACCENT, (x, 12), 4)
        else:
            pygame.draw.circle(screen, DIM, (x, 12), 4, 1)
    pygame.draw.line(screen, DIM, (10, 24), (W - 10, 24), 1)


def draw_bar(x, y, w, h, value, max_val, is_current=False, is_peak=False):
    pygame.draw.rect(screen, (40, 45, 60), (x, y, w, h))
    if not max_val or not value:
        return
    filled = int((value / max_val) * h)
    col = AMBER if is_current else (GREEN if is_peak else ACCENT)
    pygame.draw.rect(screen, col, (x, y + h - filled, w, filled))


def draw_histogram(peak, y_top):
    pygame.draw.line(screen, DIM, (10, y_top - 4), (W - 10, y_top - 4), 1)
    draw_text('TRAFFIC BY HOUR', font_sm, DIM, 10, y_top)
    if not peak:
        return
    hours   = peak['hours']
    max_cnt = max(h['count'] for h in hours) or 1
    bar_w   = (W - 20) // 24
    chart_h = 28
    chart_y = y_top + 16

    for h in hours:
        x          = 10 + h['hour'] * bar_w
        is_current = h['current']
        is_pk      = h['count'] == max_cnt and max_cnt > 0
        draw_bar(x, chart_y, bar_w - 1, chart_h, h['count'], max_cnt, is_current, is_pk)

    for hr in range(0, 24, 3):
        x = 10 + hr * bar_w
        draw_text(str(hr), font_sm, DIM, x, chart_y + chart_h + 2)


def draw_data_age(last_fetch_time, y):
    if last_fetch_time <= 0:
        return
    age = int(time.time() - last_fetch_time)
    if age < 60:
        age_str = f'{age}s AGO'
    else:
        age_str = f'{age // 60}m AGO'
    col = DIM if age < REFRESH * 2 else (AMBER if age < REFRESH * 6 else RED)
    draw_text(f'UPDATED {age_str}', font_sm, col, W - 10, y, 'right')


def temp_color(temp_c):
    if temp_c < 5:   return ACCENT
    if temp_c < 30:  return GREEN
    if temp_c < 38:  return AMBER
    return RED


def wind_color(speed_kmh):
    if speed_kmh < 20: return GREEN
    if speed_kmh < 40: return AMBER
    return RED


# ── Page 0: Flights + Weather ────────────────────────────

def render_page_flights(flights, weather, peak, page, flights_ok=True):
    screen.fill(BG)
    healthy = flights_ok or weather is not None
    draw_title_bar(healthy, page)

    content_y = 30
    weather_y = 158
    row_h = 44

    if not flights_ok and not flights:
        draw_text('-- DATA UNAVAILABLE --', font_md, RED, W // 2, 80, 'center')
        draw_text('flight fetch failed', font_sm, DIM, W // 2, 104, 'center')
    else:
        n = len(flights) if flights else 0
        header = f'FLIGHTS NEARBY ({n})'
        if not flights_ok:
            header += '  [STALE]'
        draw_text(header, font_sm, AMBER if not flights_ok else DIM, 10, content_y)
        draw_text(f'TODAY: {len(seen_today)}', font_sm, GREEN, W - 10, content_y, 'right')

        if flights:
            for i, f in enumerate(flights):
                y = content_y + 18 + i * row_h
                # Background band for row grouping
                pygame.draw.rect(screen, ROW_BG, (6, y - 2, W - 12, row_h - 4))
                # Callsign (primary, WHITE) + phase (secondary, small + colored)
                draw_text(f['callsign'], font_md, WHITE, 10, y)
                phase_col = PHASE_COLORS.get(f['phase'], DIM)
                draw_text(f['phase'], font_sm, phase_col, 180, y + 3)
                draw_text(f'{f["dist"]:.1f} KM', font_md, ACCENT, W - 10, y, 'right')
                # Route + altitude sub-line
                route = f.get('route', '')
                if route:
                    draw_text(route, font_sm, DIM, 10, y + 22)
                draw_text(format_alt(f['alt']), font_sm, DIM, W - 10, y + 22, 'right')
        else:
            clear_y = (content_y + 18 + weather_y) // 2 - 9
            draw_text('CLEAR SKIES', font_md, DIM, W // 2, clear_y, 'center')

    # Weather section
    pygame.draw.line(screen, DIM, (10, weather_y - 4), (W - 10, weather_y - 4), 1)
    draw_text('WEATHER', font_sm, DIM, 10, weather_y)

    if weather and 'temp' in weather:
        col_w = W // 3
        temp_val = weather['temp']
        draw_text(f'{temp_val:.1f}\u00b0C', font_md, temp_color(temp_val), 10, weather_y + 20)
        draw_text('TEMPERATURE', font_sm, DIM, 10, weather_y + 42)
        cond = weather.get('condition', '---')
        if len(cond) > 14:
            cond = cond[:13] + '.'
        draw_text(cond, font_md, WHITE, col_w + 10, weather_y + 20)
        draw_text('CONDITIONS', font_sm, DIM, col_w + 10, weather_y + 42)
        wind_spd = weather.get('wind_speed', 0)
        wind_dir = weather.get('wind_cardinal', '')
        draw_text(f'{wind_spd:.0f} KM/H {wind_dir}', font_md, wind_color(wind_spd), col_w * 2 + 10, weather_y + 20)
        draw_text('WIND', font_sm, DIM, col_w * 2 + 10, weather_y + 42)
    else:
        draw_text('LOADING...', font_sm, DIM, W // 2, weather_y + 28, 'center')

    draw_histogram(peak, 228)
    draw_data_age(last_fetch, 306)
    flush_to_fb()


# ── Page 1: Railway Proxy Stats ──────────────────────────

def render_page_dashboard(stats, peak, page):
    screen.fill(BG)
    draw_title_bar(stats is not None, page)

    if stats is None:
        draw_text('-- proxy unreachable --', font_md, RED, W // 2, H // 2, 'center')
        flush_to_fb()
        return

    # Stats grid (2x4)
    draw_text('RAILWAY PROXY', font_sm, DIM, 10, 30)
    if stats:
        items = [
            ('UPTIME',    stats.get('uptime', '-')),
            ('REQUESTS',  str(stats.get('totalRequests', 0))),
            ('CACHE HIT', stats.get('cacheHitRate', '-')),
            ('ERRORS',    str(stats.get('errors', 0))),
            ('CLIENTS',   str(stats.get('uniqueClients', 0))),
            ('CACHED',    str(stats.get('cacheEntries', 0)) + ' entries'),
            ('ROUTES',    str(stats.get('routeCacheEntries', 0))),
            ('UPSTREAM',  str(stats.get('activeUpstream', 0)) + ' active'),
        ]
        col_w = W // 4
        for i, (label, value) in enumerate(items):
            cx = (i % 4) * col_w + col_w // 2
            cy = 46 + (i // 4) * 42
            draw_text(label, font_sm, DIM, cx, cy, 'center')
            err_row = label == 'ERRORS' and int(stats.get('errors', 0)) > 0
            draw_text(value, font_md, RED if err_row else WHITE, cx, cy + 16, 'center')

    # API cooldowns
    cooldown_y = 138
    pygame.draw.line(screen, DIM, (10, cooldown_y - 4), (W - 10, cooldown_y - 4), 1)
    draw_text('API STATUS', font_sm, DIM, 10, cooldown_y)
    if stats:
        cooldowns = stats.get('apiCooldowns', {})
        apis = ['adsb.lol', 'adsb.fi', 'airplanes.live']
        for i, api in enumerate(apis):
            cx = 10 + i * 160
            cd = cooldowns.get(api, 0)
            is_ok = cd <= 0
            pygame.draw.circle(screen, GREEN if is_ok else AMBER, (cx + 4, cooldown_y + 24), 4)
            draw_text(api, font_sm, WHITE if is_ok else DIM, cx + 14, cooldown_y + 18)
            if not is_ok:
                draw_text(f'{cd}s', font_sm, AMBER, cx + 14, cooldown_y + 34)

    draw_histogram(peak, 228)
    draw_data_age(last_fetch, 306)
    flush_to_fb()


# ── Page 2: Server System + Devices ─────────────────────

def format_uptime_short(secs):
    if secs is None:
        return '---'
    d = int(secs) // 86400
    h = (int(secs) % 86400) // 3600
    m = (int(secs) % 3600) // 60
    if d > 0:
        return f'{d}d {h}h'
    if h > 0:
        return f'{h}h {m}m'
    return f'{m}m'


def render_page_server(server_status, echo_hb, foxtrot_hb, peak, page):
    screen.fill(BG)
    has_data = server_status is not None
    draw_title_bar(has_data, page)

    # Server system section
    draw_text('SERVER SYSTEM', font_sm, DIM, 10, 30)

    if server_status:
        col_w = W // 3

        # OS uptime
        os_up = format_uptime_short(server_status.get('uptime'))
        draw_text(os_up, font_md, WHITE, 10, 48)
        draw_text('OS UPTIME', font_sm, DIM, 10, 68)

        # CPU temp
        temp_str = server_status.get('temp', '---')
        draw_text(str(temp_str), font_md, WHITE, col_w + 10, 48)
        draw_text('CPU TEMP', font_sm, DIM, col_w + 10, 68)

        # RAM
        ram = server_status.get('ram', {})
        ram_total = ram.get('total', 0)
        ram_free = ram.get('free', 0)
        if ram_total > 0:
            ram_pct = int(((ram_total - ram_free) / ram_total) * 100)
            ram_used_mb = int((ram_total - ram_free) / 1048576)
            ram_col = GREEN if ram_pct < 70 else (AMBER if ram_pct < 90 else RED)
            draw_text(f'{ram_pct}%', font_md, ram_col, col_w * 2 + 10, 48)
            draw_text(f'RAM ({ram_used_mb} MB)', font_sm, DIM, col_w * 2 + 10, 68)
        else:
            draw_text('---', font_md, DIM, col_w * 2 + 10, 48)
            draw_text('RAM', font_sm, DIM, col_w * 2 + 10, 68)

        # Load average
        load = server_status.get('loadAvg', [])
        if load:
            load_str = ' / '.join(f'{l:.2f}' for l in load[:3])
            draw_text(load_str, font_sm, DIM, 10, 90)
            draw_text('LOAD AVG (1/5/15)', font_sm, DIM, 250, 90)
    else:
        draw_text('-- server unreachable --', font_md, RED, W // 2, 60, 'center')

    # Device heartbeats section
    device_y = 114
    pygame.draw.line(screen, DIM, (10, device_y - 4), (W - 10, device_y - 4), 1)
    draw_text('CONNECTED DEVICES', font_sm, DIM, 10, device_y)

    devices = [('ECHO', echo_hb), ('FOXTROT', foxtrot_hb)]
    for i, (name, hb) in enumerate(devices):
        bx = 10 + i * (W // 2)
        by = device_y + 18
        box_w = W // 2 - 15

        pygame.draw.rect(screen, ROW_BG, (bx, by, box_w, 72))

        if hb and hb.get('online'):
            pygame.draw.circle(screen, GREEN, (bx + 10, by + 12), 4)
            draw_text(name, font_md, WHITE, bx + 20, by + 4)

            # Firmware version
            fw = hb.get('fw', '---')
            draw_text(fw, font_sm, DIM, bx + box_w - 4, by + 6, 'right')

            # Heap + RSSI + uptime
            heap = hb.get('heap')
            if heap:
                heap_kb = int(heap) // 1024
                heap_col = GREEN if heap_kb > 100 else (AMBER if heap_kb > 50 else RED)
                draw_text(f'{heap_kb} KB', font_sm, heap_col, bx + 10, by + 26)
                draw_text('HEAP', font_sm, DIM, bx + 10, by + 40)

            rssi = hb.get('rssi')
            if rssi is not None:
                rssi_val = int(rssi)
                rssi_col = GREEN if rssi_val > -60 else (AMBER if rssi_val > -75 else RED)
                draw_text(f'{rssi_val} dBm', font_sm, rssi_col, bx + 80, by + 26)
                draw_text('RSSI', font_sm, DIM, bx + 80, by + 40)

            dev_up = hb.get('uptime')
            if dev_up is not None:
                draw_text(format_uptime_short(dev_up), font_sm, WHITE, bx + 160, by + 26)
                draw_text('UP', font_sm, DIM, bx + 160, by + 40)

            # Age
            age_sec = hb.get('ageSec', 0)
            age_col = GREEN if age_sec < 120 else (AMBER if age_sec < 600 else RED)
            draw_text(f'{age_sec}s ago', font_sm, age_col, bx + 10, by + 56)
        else:
            pygame.draw.circle(screen, RED, (bx + 10, by + 12), 4)
            draw_text(name, font_md, DIM, bx + 20, by + 4)
            draw_text('OFFLINE', font_sm, RED, bx + 10, by + 30)

    draw_histogram(peak, 228)
    draw_data_age(last_fetch, 306)
    flush_to_fb()


# ── Main loop ────────────────────────────────────────────

stats_data, peak_data, flights_raw = None, None, None
weather_data      = None
server_status     = None
echo_heartbeat    = None
foxtrot_heartbeat = None
processed_flights = []
flights_ok        = True
current_page      = 0
last_page_switch  = time.time()
last_fetch        = 0
last_wx_fetch     = 0
last_srv_fetch    = 0
SERVER_REFRESH    = 30

while True:
    now = time.time()

    if now - last_fetch >= REFRESH:
        stats_data, peak_data, flights_raw = fetch_all()
        if flights_raw is FETCH_FAILED:
            flights_ok = False
        else:
            processed_flights = process_flights(flights_raw)
            track_flights_today(processed_flights)
            flights_ok = True
        last_fetch = now

    if now - last_wx_fetch >= WEATHER_REFRESH:
        wx = fetch_weather()
        if wx is not None:
            weather_data = wx
        last_wx_fetch = now

    if now - last_srv_fetch >= SERVER_REFRESH:
        srv, echo_hb, fox_hb = fetch_server_status()
        if srv is not None:
            server_status = srv
        if echo_hb is not None:
            echo_heartbeat = echo_hb
        if fox_hb is not None:
            foxtrot_heartbeat = fox_hb
        last_srv_fetch = now

    if now - last_page_switch >= PAGE_ROTATE_SEC:
        current_page = (current_page + 1) % PAGE_COUNT
        last_page_switch = now

    if current_page == 0:
        render_page_flights(processed_flights, weather_data, peak_data, current_page, flights_ok)
    elif current_page == 1:
        render_page_dashboard(stats_data, peak_data, current_page)
    else:
        render_page_server(server_status, echo_heartbeat, foxtrot_heartbeat, peak_data, current_page)

    time.sleep(1)
