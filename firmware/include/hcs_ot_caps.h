#pragma once
/**
 * OpenTherm boiler-capability helpers for Home Climate System (portable,
 * host-testable): setpoint bounds clamping, remote-parameter gating and
 * fault-history-buffer formatting. No Arduino types.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

namespace hcs {

/** Clamp v into [lb,ub]; falls back to defaults when bounds unknown. */
inline float ot_clamp_with_bounds(float v, bool have_bounds, float lb, float ub,
                                  float def_lb, float def_ub) {
  if (!have_bounds) {
    lb = def_lb;
    ub = def_ub;
  }
  if (ub < lb) ub = lb;  // defensive
  if (v < lb) return lb;
  if (v > ub) return ub;
  return v;
}

/**
 * Slow-read round robin: returns the MsgID to fetch on cycle i.
 * Order: pressure, slave-cfg, master-cfg, capacity, DHW bounds,
 * MaxTSet bounds, FHB size, then repeats. Cheap enough for 1 slot/s.
 */
constexpr uint8_t kSlowReadCount = 8;
constexpr uint16_t kSlowReadIds[kSlowReadCount] = {
    18,  // CHPressure
    3,   // SConfigSMemberIDcode
    2,   // MConfigMMemberIDcode
    15,  // MaxCapacityMinModLevel
    48,  // TdhwSetUB/LB
    49,  // MaxTSetUB/LB  (spec: 49 = MaxTSetUBMaxTSetLB)
    57,  // MaxTSet value (the OT-side CH ceiling)
    13,  // FHBsize
};
inline uint16_t ot_slow_read_id(uint32_t cycle_i) {
  return kSlowReadIds[cycle_i % kSlowReadCount];
}

/**
 * Sanitize a head-end Max CH setpoint (ID 57) before it touches the wire.
 * Returns NAN when the value must be ignored: below the usable floor
 * (an empty/garbage MQTT payload atof()s to 0 — clamping that to 20
 * pinned a live house at 20 C in a self-triggering retained-echo loop,
 * v1.5.35), above 90, or NaN. Otherwise clamps to the boiler's stated
 * ID 49 upper bound when known. Rounds to the 0.5 grid the bus uses.
 */
inline float ot_max_tset_sanitize(float c, bool have_bounds, int8_t ub) {
  // Below the usable floor (or NaN): an empty/garbage MQTT payload atof()s
  // to 0 — reject, never clamp (clamping pinned a live house at 20 C in a
  // self-triggering retained-echo loop, v1.5.35).
  if (!(c >= 20.0f)) return (0.0f / 0.0f);
  float hi = (have_bounds && ub >= 20) ? (float)ub : 90.0f;
  if (c > hi) c = hi;
  if (c > 90.0f) c = 90.0f;
  float r = (float)((int)(c * 2.0f + 0.5f)) / 2.0f;
  return r;
}

/** True when the remote-parameter write bit for TdhwSet (D) is enabled. */
inline bool ot_rbp_dhw_write_enabled(uint8_t write_flags) {
  return write_flags & 0x10;  // parameter D -> bit 4 of write-enable byte
}

/** Format FHB raw bytes as "AA BB CC"; out needs 3*n bytes (n<=255/3). */
inline void ot_fhb_format(const uint8_t* codes, uint8_t n, char* out,
                          size_t outlen) {
  if (!out || outlen == 0) return;
  size_t p = 0;
  out[0] = '\0';
  for (uint8_t i = 0; i < n; i++) {
    int w = snprintf(out + p, outlen - p, "%s%02X", i ? " " : "", codes[i]);
    if (w < 0 || (size_t)w >= outlen - p) return;  // truncated safely
    p += w;
  }
}


/**
 * Learned CH setpoint ceiling from TSet write-ACK echoes.
 *
 * Some boilers ACK a TSet write with the value they actually stored — a
 * lower echo is their real, undocumented ceiling (a physical knob or
 * firmware limit the OT capabilities don't advertise). Three consecutive
 * lower echoes latch the cap; an echo back at/above the desired value
 * clears it, so a re-configured boiler lifts the clamp automatically.
 */
inline float ot_tset_cap_apply(bool have_cap, float cap, float want) {
  return (have_cap && cap < want) ? cap : want;
}

inline void ot_tset_echo_learn(bool& have_cap, float& cap, uint8_t& confirm,
                               float want, float echoed) {
  const float tol = 0.45f;  // 0.5 °C grid, minus float noise
  if (!have_cap) {
    if (echoed < want - tol) {
      if (++confirm >= 3) {
        have_cap = true;
        cap = echoed;
        confirm = 0;
      }
    } else {
      confirm = 0;
    }
    return;
  }
  if (echoed >= want - tol) {
    // Only a request ABOVE the latched cap, accepted in full, proves the
    // ceiling is gone. A command that was already at or below the cap
    // (the curve asking for 55 on a 55 °C boiler) must not clear it.
    if (want > cap + tol) {
      have_cap = false;
      cap = 0.0f;
      confirm = 0;
    } else {
      confirm = 0;
    }
    return;
  }
  if (echoed + tol < cap) {
    // Knob turned down: three lower echoes move the cap down with it.
    if (++confirm >= 3) {
      cap = echoed;
      confirm = 0;
    }
    return;
  }
  confirm = 0;
  if (echoed > cap + tol) cap = echoed;  // boiler raised its own ceiling
}

}  // namespace hcs
