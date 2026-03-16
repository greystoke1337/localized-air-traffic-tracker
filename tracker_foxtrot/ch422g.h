#pragma once
#include <Wire.h>

// CH422G I2C function addresses (7-bit, each function has its own address)
#define CH422G_ADDR_SET  0x24  // System config: enable EXIO output mode
#define CH422G_ADDR_IO   0x38  // Write EXIO0-7 output levels

// EXIO pin bitmasks — Waveshare ESP32-S3-Touch-LCD-4.3B mapping
#define EXIO_TP_RST   (1 << 0)  // EXIO0: Touch reset (active low)
#define EXIO_LCD_RST  (1 << 1)  // EXIO1: LCD reset (active low)
#define EXIO_LCD_BL   (1 << 2)  // EXIO2: LCD backlight (high = on)
#define EXIO_SD_CS    (1 << 4)  // EXIO4: SD card chip select (active low)
#define EXIO_USB_SEL  (1 << 5)  // EXIO5: USB select

static uint8_t _ch422g_io = 0;

static void ch422gInit() {
  Wire.begin(8, 9);  // SDA=GPIO8, SCL=GPIO9 (shared I2C bus with GT911 touch)

  // Enable EXIO0-7 as push-pull outputs
  Wire.beginTransmission(CH422G_ADDR_SET);
  Wire.write(0x01);
  Wire.endTransmission();

  // Safe defaults: resets deasserted, backlight on, SD deselected, USB normal
  _ch422g_io = EXIO_TP_RST | EXIO_LCD_RST | EXIO_LCD_BL | EXIO_SD_CS | EXIO_USB_SEL;
  Wire.beginTransmission(CH422G_ADDR_IO);
  Wire.write(_ch422g_io);
  Wire.endTransmission();

  Serial.println("CH422G expander initialized");
}

static void ch422gSetPin(uint8_t mask, bool high) {
  if (high) _ch422g_io |= mask;
  else      _ch422g_io &= ~mask;
  Wire.beginTransmission(CH422G_ADDR_IO);
  Wire.write(_ch422g_io);
  Wire.endTransmission();
}

static void ch422gResetTouch() {
  ch422gSetPin(EXIO_TP_RST, false);
  delay(10);
  ch422gSetPin(EXIO_TP_RST, true);
  delay(50);
}
