#pragma once
/**
 * Diagnostic status LED — blink patterns only (plain GPIO).
 *
 * DO NOT use neopixelWrite / RMT on ESP32-C3: Arduino's neopixelWrite()
 * calls rmt_driver_install() which abort()s under our boot path
 * (lock_acquire_generic). That bricked 1.4.6/1.4.7 before Wi‑Fi.
 *
 *   FAILSAFE  rapid strobe
 *   WIFI      fast blink
 *   NOLINK    medium blink
 *   OK        slow heartbeat
 *
 * Build flags:
 *   -DHCS_STATUS_LED_PIN=n        (omit → compiled out)
 *   -DHCS_STATUS_LED_ACTIVE_LOW
 *   -DHCS_STATUS_LED_RGB is IGNORED (kept for ini compatibility only)
 */

#include <Arduino.h>

class StatusLed {
 public:
  void begin() {
#if defined(HCS_STATUS_LED_PIN) && !defined(HCS_STATUS_LED_DISABLE)
    _pin = HCS_STATUS_LED_PIN;
    _active_low =
#if defined(HCS_STATUS_LED_ACTIVE_LOW)
        true;
#else
        false;
#endif
    // RGB / WS2812 intentionally not driven — plain digital blink only.
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, _active_low ? HIGH : LOW);
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
      case OK:     blink(now, 3000, 60);  break;
      case NOLINK: blink(now, 900, 280);  break;
      case WIFI:   blink(now, 260, 130);  break;
      case FAIL:   blink(now, 320, 160);  break;
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
    if (_pin == 255) return;
    digitalWrite(_pin, (_active_low != on) ? HIGH : LOW);
  }

  uint8_t _pin = 255;
  bool _active_low = false;
  bool _on = false;
  uint32_t _last = 0;
  Mode _mode = OK;
};
