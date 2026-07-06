// ============================================================================
//  Pantalla ESP32-32E 4"  ->  ST7796 480x320 (SPI) + touch XPT2046
//  Mismo pinout que el proyecto SPL Meter. ESP32 clasico (WROOM-32E), sin PSRAM.
// ============================================================================
#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796   _panel;
  lgfx::Bus_SPI        _bus;
  lgfx::Light_PWM      _light;
  lgfx::Touch_XPT2046  _touch;
public:
  LGFX() {
    { auto c = _bus.config();
      c.spi_host=HSPI_HOST; c.spi_mode=0;
      c.freq_write=40000000; c.freq_read=16000000;
      c.spi_3wire=false; c.use_lock=true;
      c.dma_channel=SPI_DMA_CH_AUTO;
      c.pin_sclk=14; c.pin_mosi=13; c.pin_miso=12; c.pin_dc=2;
      _bus.config(c); _panel.setBus(&_bus);
    }
    { auto c = _panel.config();
      c.pin_cs=15; c.pin_rst=-1; c.pin_busy=-1;
      c.memory_width=320; c.memory_height=480;
      c.panel_width=320;  c.panel_height=480;
      c.offset_x=0; c.offset_y=0; c.offset_rotation=0;
      c.readable=false; c.invert=false; c.rgb_order=false;
      c.dlen_16bit=false; c.bus_shared=true;
      _panel.config(c);
    }
    { auto c = _light.config();
      c.pin_bl=27; c.invert=false; c.freq=12000; c.pwm_channel=7;
      _light.config(c); _panel.setLight(&_light);
    }
    { auto c = _touch.config();
      c.x_min=300;  c.x_max=3900;
      c.y_min=200;  c.y_max=3900;
      c.pin_int=36; c.bus_shared=true;
      c.offset_rotation=1;     // misma rotacion que el LCD (1=landscape)
      c.spi_host=HSPI_HOST;
      c.freq=1000000;
      c.pin_sclk=14; c.pin_mosi=13; c.pin_miso=12; c.pin_cs=33;
      _touch.config(c);
      _panel.setTouch(&_touch);
    }
    setPanel(&_panel);
  }
};
