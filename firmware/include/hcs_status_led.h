#pragma once
/**
 * Diagnostic status LED — single addressable WS2812.
 *
 * Patterns:
 *   FAIL     red strobe (failsafe active)
 *   WIFI     fast blue blink (re-associating)
 *   NOLINK   amber medium blink (OT no-link)
 *   OK       slow green heartbeat
 *
 * Brightness (1-255, default 64 = 25%) + enable/disable via MQTT (hcs/<node>/set/led)
 * and POST /api/control {"key":"led","value":"on" | "off" | "<brightness>"}
 * persisted in HcsSettings.led_enable / led_brightness via Preferences.
 *
 * neopixelWrite is safe to call from loop() context once setup() has completed.
 *
 * Build flags:
 *   -DHCS_STATUS_LED_PIN=n    (omit → compiled out)
 *   -DHCS_STATUS_LED_DISABLE  (set in platformio.ini for boards without an LED)
 */
#include <Arduino.h>

class StatusLed {
 public:
  void begin() {
#if defined(HCS_STATUS_LED_PIN) && !defined(HCS_STATUS_LED_DISABLE) && defined(ESP32)
    _pin = HCS_STATUS_LED_PIN;
    pinMode(_pin, OUTPUT);
    neopixelWrite(_pin, 0, 0, 0);  // dark at boot until update() sets pattern
#else
    _pin = 255;
#endif
  }

  void schedulePattern(bool wifi_ok, bool ot_valid, bool failsafe) {
#if defined(HCS_STATUS_LED_PIN) && !defined(HCS_STATUS_LED_DISABLE) && defined(ESP32)
    if (_pin == 255) return;
    _wifi_ok = wifi_ok; _ot_valid = ot_valid; _failsafe = failsafe;
    Mode m = failsafe ? FAIL : (!wifi_ok ? WIFI : (!ot_valid ? NOLINK : OK));
    if (m != _mode) {
      _mode = m;
      _last = 0;  // reset so the new pattern starts on the next update()
      _on = false;
    }
#endif
  }

  void update(bool wifi_ok, bool ot_valid, bool failsafe) {
    schedulePattern(wifi_ok, ot_valid, failsafe);
#if defined(HCS_STATUS_LED_PIN) && !defined(HCS_STATUS_LED_DISABLE) && defined(ESP32)
    if (_pin == 255 || !_enabled) return;
    uint32_t now = millis();
    switch (_mode) {
      case OK:     blink(now, 3000, 60);  break;
      case NOLINK: blink(now, 900, 280);  break;
      case WIFI:   blink(now, 260, 130);  break;
      case FAIL:   blink(now, 320, 160);  break;
    }
#endif
  }

  uint8_t brightness()       const { return _brightness; }
  void    setBrightness(uint8_t b) { _brightness = b; }
  bool    enabled()          const { return _enabled; }
  void    setEnabled(bool on) {
    _enabled = on;
    if (!on) paint_off();
  }

 private:
  enum Mode : uint8_t { OK, NOLINK, WIFI, FAIL };

  void colorOf_(Mode m, uint8_t& r, uint8_t& g, uint8_t& b) const {
    switch (m) {
      case OK:     r = 0;   g = 96;  b = 0;   break;
      case NOLINK: r = 255; g = 176; b = 59;  break;
      case WIFI:   r = 0;   g = 0;   b = 255; break;
      case FAIL:   r = 255; g = 30;  b = 0;   break;
    }
  }

  void blink(uint32_t now, uint32_t period_ms, uint32_t on_ms) {
    if (now - _last >= period_ms) _last = now;
    bool on = (now - _last) < on_ms;
    if (on == _on) return;
    _on = on;
    if (!on) { paint_off(); return; }
#if defined(HCS_STATUS_LED_PIN) && !defined(HCS_STATUS_LED_DISABLE) && defined(ESP32)
    uint8_t r, g, b;
    colorOf_(_mode, r, g, b);
    uint8_t dim = (uint32_t)_brightness * _brightness / 255;
    neopixelWrite(_pin, (r * dim) >> 8, (g * dim) >> 8, (b * dim) >> 8);
#endif
  }

  void paint_off() {
#if defined(HCS_STATUS_LED_PIN) && !defined(HCS_STATUS_LED_DISABLE) && defined(ESP32)
    neopixelWrite(_pin, 0, 0, 0);
#endif
  }

  bool     _enabled = true;
  uint8_t  _brightness = 64;
  uint8_t  _pin = 255;
  bool     _on = false;
  uint32_t _last = 0;
  Mode     _mode = OK;
  bool     _wifi_ok = false;
  bool     _ot_valid = false;
  bool     _failsafe = false;
};
