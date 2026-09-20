#pragma once
/**
 * Diagnostic status LED — two hardware families:
 *   • WS2812 (default via -DHCS_STATUS_LED_WS2812)  → neopixelWrite, colour
 *     patterns + brightness (C3 mini v2.1, WS2812 on GPIO7).
 *   • plain digital LED (-DHCS_STATUS_LED_PLAIN)    → digitalWrite blink,
 *     single-colour patterns on timing only (D1 mini ESP32 GPIO2,
 *     S2 mini LED1 GPIO15). Polarity via -DHCS_STATUS_LED_ACTIVE_LOW.
 *
 * Patterns (period/on-ms):
 *   OK       3000/60    slow heartbeat
 *   NOLINK    900/280   no OpenTherm link
 *   WIFI      260/130   re-associating
 *   FAIL      320/160   failsafe strobe
 *
 * Brightness (1-255, default 64) applies to WS2812 only; plain LEDs blink
 * at full strength. on/off + brightness via MQTT (hcs/<node>/set/led) and
 * POST /api/control {"led": ...}; persisted in HcsSettings.
 *
 * Build flags:
 *   -DHCS_STATUS_LED_PIN=n            (omit → compiled out)
 *   -DHCS_STATUS_LED_WS2812           addressable RGB (C3 mini v2.1)
 *   -DHCS_STATUS_LED_PLAIN            single-colour digital LED
 *   -DHCS_STATUS_LED_ACTIVE_LOW       LED lights when pin is LOW
 *   -DHCS_STATUS_LED_DISABLE          compiled out regardless
 */
#include <Arduino.h>

class StatusLed {
 public:
  void begin() {
#if defined(HCS_STATUS_LED_PIN) && !defined(HCS_STATUS_LED_DISABLE) && defined(ESP32)
    _pin = HCS_STATUS_LED_PIN;
    pinMode(_pin, OUTPUT);
    paint_off();
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
#if defined(HCS_STATUS_LED_WS2812)
    uint8_t r, g, b;
    colorOf_(_mode, r, g, b);
    uint8_t dim = (uint32_t)_brightness * _brightness / 255;
    neopixelWrite(_pin, (r * dim) >> 8, (g * dim) >> 8, (b * dim) >> 8);
#elif defined(HCS_STATUS_LED_PLAIN)
    digitalWrite(_pin, active_high_() ? HIGH : LOW);
#endif
#endif
  }

  void paint_off() {
#if defined(HCS_STATUS_LED_PIN) && !defined(HCS_STATUS_LED_DISABLE) && defined(ESP32)
#if defined(HCS_STATUS_LED_WS2812)
    neopixelWrite(_pin, 0, 0, 0);
#elif defined(HCS_STATUS_LED_PLAIN)
    digitalWrite(_pin, active_high_() ? LOW : HIGH);
#endif
#endif
  }

 private:
  static constexpr bool active_high_() {
#if defined(HCS_STATUS_LED_ACTIVE_LOW)
    return false;
#else
    return true;
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
