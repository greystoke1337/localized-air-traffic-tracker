#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796  _panel_instance;
  lgfx::Bus_SPI       _bus_instance;
  lgfx::Touch_XPT2046 _touch_instance;
  lgfx::Light_PWM     _light_instance;

public:
  LGFX(void) {
    // SPI bus — HSPI, matching Freenove FNK0103S pin layout
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host  = HSPI_HOST;
      cfg.spi_mode  = 0;
      cfg.freq_write = 80000000;
      cfg.freq_read  = 20000000;
      cfg.pin_sclk  = 14;
      cfg.pin_mosi  = 13;
      cfg.pin_miso  = 12;
      cfg.pin_dc    =  2;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    // Panel — ST7796 480x320
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs        = 15;
      cfg.pin_rst       = -1;   // tied to board RST
      cfg.pin_busy      = -1;
      cfg.memory_width  = 320;
      cfg.memory_height = 480;
      cfg.panel_width   = 320;
      cfg.panel_height  = 480;
      cfg.offset_x      = 0;
      cfg.offset_y      = 0;
      cfg.offset_rotation = 0;
      cfg.readable      = true;
      cfg.invert        = false;
      cfg.rgb_order     = false; // BGR (ST7796 default)
      cfg.dlen_16bit    = false;
      cfg.bus_shared    = true;  // shared SPI with touch + SD
      _panel_instance.config(cfg);
    }

    // Touch — XPT2046 resistive, shares SPI bus
    {
      auto cfg = _touch_instance.config();
      cfg.x_min       = 300;
      cfg.x_max       = 3900;
      cfg.y_min       = 400;
      cfg.y_max       = 3900;
      cfg.pin_int     = -1;
      cfg.bus_shared  = true;
      cfg.offset_rotation = 0;
      cfg.spi_host    = HSPI_HOST;
      cfg.freq        = 2500000;
      cfg.pin_sclk    = 14;
      cfg.pin_mosi    = 13;
      cfg.pin_miso    = 12;
      cfg.pin_cs      = 33;
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }

    // Backlight — PWM on GPIO 27 (active HIGH)
    {
      auto cfg = _light_instance.config();
      cfg.pin_bl      = 27;
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 7;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    setPanel(&_panel_instance);
  }
};
