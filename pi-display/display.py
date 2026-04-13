import os, sys, time, json, threading
import pygame
import numpy as np

try:
    import evdev
    _EVDEV_AVAILABLE = True
except ImportError:
    _EVDEV_AVAILABLE = False

os.environ['SDL_VIDEODRIVER'] = 'dummy'
os.environ['SDL_NOMOUSE']     = '1'

# ── Colours ──────────────────────────────────────────────────────────────────
BG       = (  4,   8,  20)
HDR_BG   = (  0,  25,  70)
WHITE    = (200, 220, 255)
CYAN     = (  0, 185, 255)
BLUE     = ( 50, 120, 200)
BLUE_DIM = ( 18,  45, 100)
AMBER    = (255, 160,   0)
RED      = (210,  45,  45)

FONT_PATH = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'

# ── CPU% state ────────────────────────────────────────────────────────────────
_prev_cpu_idle  = None
_prev_cpu_total = None

# ── Data readers ──────────────────────────────────────────────────────────────

def read_cpu_temp():
    with open('/sys/class/thermal/thermal_zone0/temp') as f:
        return int(f.read().strip()) / 1000.0


def read_cpu_pct():
    global _prev_cpu_idle, _prev_cpu_total
    with open('/proc/stat') as f:
        fields = list(map(int, f.readline().split()[1:]))
    idle  = fields[3]
    total = sum(fields)
    if _prev_cpu_total is None:
        _prev_cpu_idle, _prev_cpu_total = idle, total
        return 0.0
    d_idle  = idle  - _prev_cpu_idle
    d_total = total - _prev_cpu_total
    _prev_cpu_idle, _prev_cpu_total = idle, total
    if d_total == 0:
        return 0.0
    return max(0.0, min(100.0, (1.0 - d_idle / d_total) * 100.0))


def read_memory():
    info = {}
    with open('/proc/meminfo') as f:
        for line in f:
            k, v = line.split(':', 1)
            info[k.strip()] = int(v.split()[0])
    total_mb = info['MemTotal'] // 1024
    used_mb  = (info['MemTotal'] - info['MemAvailable']) // 1024
    pct      = used_mb * 100 // total_mb if total_mb else 0
    return used_mb, total_mb, pct


def read_uptime():
    with open('/proc/uptime') as f:
        secs = float(f.read().split()[0])
    days  = int(secs // 86400)
    hours = int((secs % 86400) // 3600)
    mins  = int((secs % 3600) // 60)
    return days, hours, mins


def read_telemetry():
    try:
        with open('/run/readsb/stats.json') as f:
            s = json.load(f)
        lm  = s.get('last1min', {})
        loc = lm.get('local', {})
        return {
            'msgs_min': lm.get('messages_valid', 0),
            'signal':   loc.get('signal', 0.0),
            'noise':    loc.get('noise', 0.0),
            'strong':   loc.get('strong_signals', 0),
            'tracks':   lm.get('tracks', {}).get('all', 0),
        }
    except Exception:
        return {'msgs_min': 0, 'signal': 0.0, 'noise': 0.0, 'strong': 0, 'tracks': 0}


def read_aircraft():
    try:
        with open('/run/readsb/aircraft.json') as f:
            data = json.load(f)
        aircraft = data.get('aircraft', [])
        total    = len(aircraft)
        with_pos = [a for a in aircraft if 'lat' in a and 'lon' in a and 'r_dst' in a]

        furthest = max(with_pos, key=lambda a: a.get('r_dst', 0), default=None)
        nearest  = min(with_pos, key=lambda a: a.get('r_dst', 0), default=None)

        with_alt = [a for a in aircraft if isinstance(a.get('alt_baro'), (int, float))]
        highest  = max(with_alt, key=lambda a: a.get('alt_baro', 0), default=None)

        EMRG_SQUAWKS = {'7500', '7600', '7700'}
        emergencies = [
            a for a in aircraft
            if a.get('emergency', 'none') not in ('none', '', None)
            or str(a.get('squawk', '')) in EMRG_SQUAWKS
        ]

        return {
            'total':       total,
            'with_pos':    len(with_pos),
            'furthest':    furthest,
            'highest':     highest,
            'nearest':     nearest,
            'emergencies': emergencies,
        }
    except Exception:
        return {
            'total': 0, 'with_pos': 0,
            'furthest': None, 'highest': None, 'nearest': None,
            'emergencies': [],
        }


# ── Drawing helpers ───────────────────────────────────────────────────────────

def callsign(ac):
    return ac.get('flight', ac.get('hex', '?')).strip() if ac else '—'


def draw_header(surface, fonts):
    pygame.draw.rect(surface, HDR_BG, (0, 0, 480, 36))
    t = fonts['hdr'].render('ADS-B RECEIVER', False, WHITE, HDR_BG)
    surface.blit(t, (14, 7))
    pygame.draw.line(surface, CYAN, (0, 36), (480, 36), 1)


def draw_sec(surface, fonts, x, y, text):
    surface.blit(fonts['sec'].render(text, False, BLUE, BG), (x, y))


def draw_row(surface, fonts, lx, vx, y, label, value, vc=CYAN):
    surface.blit(fonts['lbl'].render(label, False, BLUE, BG), (lx, y))
    surface.blit(fonts['val'].render(value, False, vc,   BG), (vx, y))


# ── Layout constants ──────────────────────────────────────────────────────────
L_LBL, L_VAL = 10, 115   # left col: label x, value x
R_LBL, R_VAL = 250, 335  # right col: label x, value x
DIV_X        = 242        # vertical divider x


# ── Dashboard ─────────────────────────────────────────────────────────────────

def draw_dashboard(surface, fonts):
    temp              = read_cpu_temp()
    cpu_pct           = read_cpu_pct()
    used_mb, total_mb, mem_pct = read_memory()
    days, hours, mins = read_uptime()
    t                 = read_telemetry()
    ac                = read_aircraft()

    temp_col = RED   if temp    >= 75 else AMBER if temp    >= 60 else CYAN
    cpu_col  = RED   if cpu_pct >= 80 else AMBER if cpu_pct >= 60 else CYAN
    mem_col  = RED   if mem_pct >= 85 else AMBER if mem_pct >= 70 else CYAN
    sig_col  = AMBER if t['signal'] > -10 else CYAN

    uptime_str = f'{days}d {hours}h {mins}m' if days else f'{hours}h {mins}m'

    pygame.draw.line(surface, BLUE_DIM, (DIV_X, 40), (DIV_X, 312), 1)

    # ── LEFT: HEALTH ─────────────────────────────────────────────────────────
    draw_sec(surface, fonts, L_LBL,  40, 'HEALTH')
    draw_row(surface, fonts, L_LBL, L_VAL,  58, 'TEMP',    f'{temp:.1f}\u00b0C',       temp_col)
    draw_row(surface, fonts, L_LBL, L_VAL,  78, 'CPU',     f'{cpu_pct:.0f}%',          cpu_col)
    draw_row(surface, fonts, L_LBL, L_VAL,  98, 'MEMORY',  f'{used_mb}/{total_mb} MB', mem_col)
    draw_row(surface, fonts, L_LBL, L_VAL, 118, 'UPTIME',  uptime_str)

    pygame.draw.line(surface, BLUE_DIM, (L_LBL, 142), (DIV_X - 8, 142), 1)

    # ── LEFT: RECEIVER ───────────────────────────────────────────────────────
    draw_sec(surface, fonts, L_LBL, 146, 'RECEIVER')
    draw_row(surface, fonts, L_LBL, L_VAL, 164, 'MSGS/MIN', f"{t['msgs_min']:,}")
    draw_row(surface, fonts, L_LBL, L_VAL, 184, 'SIGNAL',   f"{t['signal']:.1f} dBFS", sig_col)
    draw_row(surface, fonts, L_LBL, L_VAL, 204, 'NOISE',    f"{t['noise']:.1f} dBFS")
    draw_row(surface, fonts, L_LBL, L_VAL, 224, 'STRONG',   f"{t['strong']} signals",
             RED if t['strong'] > 0 else CYAN)
    draw_row(surface, fonts, L_LBL, L_VAL, 244, 'TRACKS',   f"{t['tracks']} aircraft")

    # ── RIGHT: AIRCRAFT ──────────────────────────────────────────────────────
    draw_sec(surface, fonts, R_LBL,  40, 'AIRCRAFT')
    draw_row(surface, fonts, R_LBL, R_VAL,  58, 'TOTAL',
             f"{ac['total']}  ({ac['with_pos']} pos)")

    if ac['furthest']:
        a = ac['furthest']
        draw_row(surface, fonts, R_LBL, R_VAL,  78, 'FURTHEST',
                 f"{callsign(a)}  {a['r_dst']:.1f}nm")
    else:
        draw_row(surface, fonts, R_LBL, R_VAL,  78, 'FURTHEST', '\u2014')

    if ac['highest']:
        a = ac['highest']
        draw_row(surface, fonts, R_LBL, R_VAL,  98, 'HIGHEST',
                 f"{callsign(a)}  FL{a['alt_baro'] // 100:03d}")
    else:
        draw_row(surface, fonts, R_LBL, R_VAL,  98, 'HIGHEST', '\u2014')

    if ac['nearest']:
        a = ac['nearest']
        draw_row(surface, fonts, R_LBL, R_VAL, 118, 'NEAREST',
                 f"{callsign(a)}  {a['r_dst']:.1f}nm")
    else:
        draw_row(surface, fonts, R_LBL, R_VAL, 118, 'NEAREST', '\u2014')

    # Emergency row — always rendered; RED when active, dim dash when clear
    pygame.draw.line(surface, BLUE_DIM, (R_LBL, 145), (472, 145), 1)
    if ac['emergencies']:
        e  = ac['emergencies'][0]
        sq = e.get('squawk', '')
        draw_row(surface, fonts, R_LBL, R_VAL, 160, '!! EMRG',
                 f"{callsign(e)}  {sq}", RED)
    else:
        draw_row(surface, fonts, R_LBL, R_VAL, 160, 'EMRG', '\u2014')

    print(f"HEALTH temp={temp:.1f} cpu={cpu_pct:.0f}% mem={mem_pct}% up={uptime_str} | "
          f"RECV msgs={t['msgs_min']} sig={t['signal']:.1f} strong={t['strong']} | "
          f"AC total={ac['total']} pos={ac['with_pos']} emrg={len(ac['emergencies'])}",
          flush=True)


# ── Framebuffer flush ─────────────────────────────────────────────────────────

def flush(surface, fb):
    arr    = pygame.surfarray.array3d(surface).transpose(1, 0, 2)
    arr    = np.rot90(arr, 2)
    r      = arr[:, :, 0].astype(np.uint16)
    g      = arr[:, :, 1].astype(np.uint16)
    b      = arr[:, :, 2].astype(np.uint16)
    rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    fb.seek(0)
    fb.write(rgb565.astype('<u2').tobytes())
    fb.flush()


# ── Tap-to-toggle screen on/off ───────────────────────────────────────────────

_BL_GPIO     = 24
_BL_GPIO_DIR = f'/sys/class/gpio/gpio{_BL_GPIO}'

def set_backlight(on):
    try:
        if not os.path.exists(_BL_GPIO_DIR):
            with open('/sys/class/gpio/export', 'w') as f:
                f.write(str(_BL_GPIO))
            time.sleep(0.05)
        with open(f'{_BL_GPIO_DIR}/direction', 'w') as f:
            f.write('out')
        with open(f'{_BL_GPIO_DIR}/value', 'w') as f:
            f.write('1' if on else '0')
    except OSError:
        pass


display_on     = True
_last_tap_time = 0.0
_TAP_DEBOUNCE  = 0.5


def _touch_listener():
    global display_on, _last_tap_time
    if not _EVDEV_AVAILABLE:
        print("evdev not available, touch disabled", file=sys.stderr, flush=True)
        return
    try:
        device = evdev.InputDevice('/dev/input/event2')
        print(f"Touch: listening on {device.path} ({device.name})", file=sys.stderr, flush=True)
    except (OSError, PermissionError) as e:
        print(f"Touch: could not open /dev/input/event2: {e}", file=sys.stderr, flush=True)
        return
    for event in device.read_loop():
        if (event.type == evdev.ecodes.EV_ABS and
                event.code == evdev.ecodes.ABS_PRESSURE and
                event.value > 0):
            now = time.time()
            if now - _last_tap_time >= _TAP_DEBOUNCE:
                _last_tap_time = now
                display_on = not display_on
                set_backlight(display_on)
                print(f"Touch: display {'ON' if display_on else 'OFF'}", file=sys.stderr, flush=True)


# ── Main ──────────────────────────────────────────────────────────────────────

pygame.init()
screen = pygame.display.set_mode((480, 320))

fonts = {
    'hdr': pygame.font.Font(FONT_PATH, 20),
    'sec': pygame.font.Font(FONT_PATH, 13),
    'lbl': pygame.font.Font(FONT_PATH, 13),
    'val': pygame.font.Font(FONT_PATH, 16),
}

threading.Thread(target=_touch_listener, daemon=True).start()

with open('/dev/fb0', 'wb') as fb:
    while True:
        if display_on:
            screen.fill(BG)
            draw_header(screen, fonts)
            draw_dashboard(screen, fonts)
            flush(screen, fb)
        time.sleep(2)
