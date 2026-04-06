#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "board.h"

class LGFX : public lgfx::LGFX_Device {
#ifdef BOARD_2P8
  lgfx::Panel_ST7789  _panel_instance;
#else
  lgfx::Panel_ST7796  _panel_instance;
  lgfx::Touch_XPT2046 _touch_instance;
#endif
  lgfx::Bus_SPI       _bus_instance;
  lgfx::Light_PWM     _light_instance;

public:
  LGFX(void) {
    // SPI bus — HSPI, shared pin layout across Freenove boards
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host  = HSPI_HOST;
      cfg.spi_mode  = 0;
#ifdef BOARD_2P8
      cfg.freq_write = 40000000;   // 40 MHz (safer for ST7789)
      cfg.freq_read  = 16000000;
      cfg.pin_miso  = -1;          // no MISO on 2.8" board
#else
      cfg.freq_write = 80000000;
      cfg.freq_read  = 20000000;
      cfg.pin_miso  = 12;
#endif
      cfg.pin_sclk  = 14;
      cfg.pin_mosi  = 13;
      cfg.pin_dc    =  2;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    // Panel
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs        = 15;
      cfg.pin_rst       = -1;   // tied to board RST
      cfg.pin_busy      = -1;
#ifdef BOARD_2P8
      cfg.memory_width  = 240;
      cfg.memory_height = 320;
      cfg.panel_width   = 240;
      cfg.panel_height  = 320;
      cfg.readable      = false;
      cfg.invert        = true;   // ST7789 typically needs inversion
      cfg.rgb_order     = true;   // ST7789 is RGB (not BGR)
#else
      cfg.memory_width  = 320;
      cfg.memory_height = 480;
      cfg.panel_width   = 320;
      cfg.panel_height  = 480;
      cfg.readable      = true;
      cfg.invert        = false;
      cfg.rgb_order     = false;  // BGR (ST7796 default)
#endif
      cfg.offset_x      = 0;
      cfg.offset_y      = 0;
      cfg.offset_rotation = 0;
      cfg.dlen_16bit    = false;
#ifdef BOARD_2P8
      cfg.bus_shared    = false;
#else
      cfg.bus_shared    = true;   // shared SPI with touch + SD
#endif
      _panel_instance.config(cfg);
    }

#ifndef BOARD_2P8
    // Touch — XPT2046 resistive, shares SPI bus (4.0" board only)
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
#endif

    // Backlight — PWM (different GPIO per board)
    {
      auto cfg = _light_instance.config();
#ifdef BOARD_2P8
      cfg.pin_bl      = 21;
#else
      cfg.pin_bl      = 27;
#endif
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 7;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    setPanel(&_panel_instance);
  }
};
