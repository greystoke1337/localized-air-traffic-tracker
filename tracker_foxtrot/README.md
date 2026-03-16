# Foxtrot Firmware — Waveshare ESP32-S3-Touch-LCD-4.3B

Arduino firmware for the [Waveshare ESP32-S3-Touch-LCD-4.3B](https://www.waveshare.com/esp32-s3-touch-lcd-4.3b.htm) (800x480 IPS, capacitive touch, ESP32-S3-WROOM-1-N16R8).

## GPIO 10 / SD Card Blue Tint Fix

**If your display has a blue or purple tint after calling `SD.begin()`, this is why.**

On this board, GPIO 10 is used by the RGB parallel display bus as the **most significant blue bit (B7)**. Many SD card examples — and even some Waveshare demos — use GPIO 10 as the SD chip select (`SD_CS`). Calling `SD.begin(10, SPI)` reconfigures GPIO 10 as a SPI chip select, which corrupts the blue channel and produces a permanent blue tint on the display.

### Root cause

The 16-bit RGB565 parallel bus uses these GPIOs for the blue channel:

| Signal | GPIO | Bit weight |
|--------|------|------------|
| B3 (LSB) | 14 | 1 |
| B4 | 38 | 2 |
| B5 | 18 | 4 |
| B6 | 17 | 8 |
| **B7 (MSB)** | **10** | **16** |

When `SD.begin(10)` takes over GPIO 10, the display loses its most significant blue bit. The result is a strong blue tint because the LCD controller reads unpredictable values on that pin during every pixel clock cycle.

### The fix — use the CH422G I/O expander

The SD card's chip select on this board is **not directly wired to any ESP32 GPIO**. It is routed through the **CH422G I/O expander** at EXIO4. The correct approach:

```cpp
#include "ch422g.h"   // Minimal CH422G I2C driver (included in this project)

// 1. Initialize the CH422G expander (I2C on GPIO 8/9)
ch422gInit();

// 2. Assert SD chip select via the expander (active low)
ch422gSetPin(EXIO_SD_CS, false);

// 3. Init SPI without touching GPIO 10
SPI.begin(12, 13, 11);  // SCK, MISO, MOSI

// 4. Use a dummy CS pin for the Arduino SD library
//    GPIO 6 (CAN TX) is unused and safe to use here
if (SD.begin(6, SPI)) {
    Serial.println("SD card ready");
}
```

### What NOT to use as dummy CS

- **GPIO 10** — RGB display blue B7 (causes blue tint)
- **GPIO 34** — FSPICS0, the SPI flash chip select (causes immediate crash / watchdog timeout — the CPU can't execute code if flash is deselected)
- **Any GPIO used by the RGB bus** — see `lgfx_config.h` for the full pin mapping

### CH422G I/O expander pinout

The CH422G is an I2C GPIO expander on the Waveshare board (SDA=GPIO 8, SCL=GPIO 9). It uses per-function I2C addresses instead of register offsets:

| Address | Function |
|---------|----------|
| 0x24 | System config (enable push-pull output mode) |
| 0x38 | Write EXIO0-7 output levels |

| EXIO pin | Function |
|----------|----------|
| EXIO0 | Touch reset (active low) |
| EXIO1 | LCD reset (active low) |
| EXIO2 | LCD backlight (high = on) |
| EXIO4 | **SD card chip select (active low)** |
| EXIO5 | USB select |

### Symptoms checklist

If you see any of these, check your SD_CS pin assignment:

- Blue or purple tint on the display that wasn't there before
- Tint appears only after `SD.begin()` or `SPI.begin()` runs
- Tint is absent in display-only test sketches
- Display looks correct until WiFi/SD code is added

### Keywords for search

Waveshare ESP32-S3-Touch-LCD-4.3 blue tint, GPIO 10 SD card conflict, RGB parallel display blue channel, CH422G SD_CS, ESP32-S3 display color shift, SD.begin blue screen.
