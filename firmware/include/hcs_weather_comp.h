#pragma once
/**
 * Weather compensation for Home Climate System firmware.
 *
 * Portable heating-curve math + MQTT payload parsing (plain C/C++, no
 * Arduino dependencies) so the exact code running on ESP8266/ESP32 is
 * unit-tested on the host with `pio test -e native`.
 *
 * Linear curve:
 *   t_out >= t_out_ref      -> flow_min  (mild outside, minimum flow)
 *   t_out <= t_out_design   -> flow_max  (design-day, maximum flow)
 *   between                 -> linear interpolation, clamped
 *
 * Config CSV (weather_comp_cfg topic): "<ref>,<design>,<fmax>,<fmin>"
 * e.g. "18,-10,65,25" = ref 18 °C, design -10 °C, flow 65..25 °C.
 */

#include <math.h>
#include <stdlib.h>

struct HcsWeatherComp {
  bool enable = false;
  float t_out_ref = 18.0f;    // outdoor °C where demand reaches zero
  float t_out_design = -10.0f; // design outdoor °C (flow = flow_max)
  float flow_max = 65.0f;      // flow setpoint at/below design temp
  float flow_min = 25.0f;      // floor when mild
};

/** Effective target flow °C, or NAN if disabled / no valid outdoor reading. */
inline float hcs_weather_comp_target(const HcsWeatherComp& wc, float t_out) {
  if (!wc.enable || isnan(t_out)) return NAN;

  float lo = (wc.t_out_design < wc.t_out_ref) ? wc.t_out_design : wc.t_out_ref;
  float hi = (wc.t_out_design < wc.t_out_ref) ? wc.t_out_ref : wc.t_out_design;
  float fmax = (wc.flow_max >= wc.flow_min) ? wc.flow_max : wc.flow_min;
  float fmin = (wc.flow_max >= wc.flow_min) ? wc.flow_min : wc.flow_max;

  if (t_out >= hi) return fmin;
  if (t_out <= lo) return fmax;

  float frac = (hi - t_out) / (hi - lo);
  return fmin + frac * (fmax - fmin);
}

/**
 * Parse "<ref>,<design>,<fmax>,<fmin>" into cfg (enable untouched).
 * Returns false and leaves cfg unchanged on malformed/contradictory input.
 */
inline bool hcs_weather_comp_parse_cfg(const char* csv, HcsWeatherComp& cfg) {
  if (!csv) return false;
  float v[4];
  const char* p = csv;
  for (int i = 0; i < 4; i++) {
    char* end = nullptr;
    v[i] = strtof(p, &end);
    if (end == p) return false;
    p = end;
    if (i < 3) {
      while (*p == ' ') p++;
      if (*p != ',') return false;
      p++;
    }
  }
  if (v[2] < v[3]) return false;   // flow_max must be >= flow_min
  if (v[1] >= v[0]) return false;  // design must be below reference
  if (v[2] > 90.0f) return false;  // keep inside boiler-safe range
  if (v[3] < 10.0f) return false;
  cfg.t_out_ref = v[0];
  cfg.t_out_design = v[1];
  cfg.flow_max = v[2];
  cfg.flow_min = v[3];
  return true;
}
