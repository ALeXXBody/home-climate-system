#pragma once
/**
 * Diagnostic status LED — one glance tells you what the board is doing.
 *
 * Non-blocking millis() state machine; call update() every loop pass.
 * Priority, worst first:
 *
 *   FAILSAFE  rapid red strobe        boiler link dead + safe mode active
 *   WIFI      fast blink (blue)       no WiFi / portal running
 *   NOLINK    red flash               WiFi fine, OpenTherm not responding
 *   OK        green heartbeat blip    everything nominal
 *
 * Single-color LEDs use the blink rhythm; WS2812 boards also get color.
 * Configure per board via build flags:
 *   -DHCS_STATUS_LED_PIN=n        (omit → feature compiled out)
 *   -DHCS_STATUS_LED_ACTIVE_LOW   (LED between pin and 3V3)
 *   -DHCS_STATUS_LED_RGB          (addressable WS2812, neopixelWrite)
 *
 * Verified onboard LEDs:
 *   d1_mini        GPIO2  active-LOW   (D4 pad; strap-safe after boot)
 *   esp32_d1_mini  GPIO2  active-HIGH  (blue LED, opposite of the 8266!)
 *   lolin_s2_mini  GPIO15 active-LOW   (schematic LED1)
 *   lolin_c3_mini  GPIO7  WS2812 RGB
 *   esp32s3_zero   GPIO48 WS2812 RGB
 */

#include <Arduino.h>

class StatusLed {
 public:
  void begin() {
#if defined(HCS_STATUS_LED_PIN)
    _pin = HCS_STATUS_LED_PIN;
    _active_low =
#if defined(HCS_STATUS_LED_ACTIVE_LOW)
        true;
#else
        false;
#endif
    _rgb =
#if defined(HCS_STATUS_LED_RGB)
        true;
#else
        false;
#endif
    pinMode(_pin, OUTPUT);
    write(false);
#else
    _pin = 255;
#endif
  }

  void update(bool wifi_ok, bool ot_valid, bool failsafe) {
    if (_pin == 255) return;
    uint32_t now = millis();
    Mode m = failsafe ? FAIL : (!wifi_ok ? WIFI : (!ot_valid ? NOLINK : OK));
    if (m != _mode) {
      _mode = m;
      _last = now;
      _on = false;
      write(false);
    }
    switch (_mode) {
      case OK:     blink(now, 3000, 60);  break;  // gentle heartbeat
      case NOLINK: blink(now, 900, 280);  break;  // attention: no boiler
      case WIFI:   blink(now, 260, 130);  break;  // fast: connecting/AP
      case FAIL:   blink(now, 320, 160);  break;  // strobe: failsafe live
    }
  }

 private:
  enum Mode : uint8_t { OK, NOLINK, WIFI, FAIL };

  void blink(uint32_t now, uint32_t period_ms, uint32_t on_ms) {
    if (now - _last >= period_ms) _last = now;
    bool on = (now - _last) < on_ms;
    if (on != _on) {
      _on = on;
      write(on);
    }
  }

  void write(bool on) {
    if (_rgb) {
#if defined(HCS_STATUS_LED_RGB)
      // Rate-limit WS2812 updates — bit-bang disables IRQs briefly and on
      // ESP32-C3 that collides with the OpenTherm bus ISR → random PANIC.
      static uint32_t last_ws = 0;
      static bool last_on = false;
      static Mode last_mode = OK;
      uint32_t now = millis();
      if (on == last_on && _mode == last_mode && (now - last_ws) < 50)
        return;
      last_ws = now;
      last_on = on;
      last_mode = _mode;
      noInterrupts();
      switch (_mode) {
        case OK:     neopixelWrite(_pin, 0, on ? 48 : 0, 0);   break;  // green
        case NOLINK: neopixelWrite(_pin, on ? 96 : 0, 0, 0);   break;  // red
        case WIFI:   neopixelWrite(_pin, 0, 0, on ? 96 : 0);   break;  // blue
        case FAIL:   neopixelWrite(_pin, on ? 160 : 0, on ? 40 : 0, 0); break;
      }
      interrupts();
#endif
    } else {
      digitalWrite(_pin, (_active_low != on) ? HIGH : LOW);
    }
  }

  uint8_t _pin = 255;
  bool _active_low = false;
  bool _rgb = false;
  bool _on = false;
  uint32_t _last = 0;
  Mode _mode = OK;
};
