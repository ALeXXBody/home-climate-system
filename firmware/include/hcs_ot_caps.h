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
constexpr uint8_t kSlowReadCount = 7;
constexpr uint16_t kSlowReadIds[kSlowReadCount] = {
    18,  // CHPressure
    3,   // SConfigSMemberIDcode
    2,   // MConfigMMemberIDcode
    15,  // MaxCapacityMinModLevel
    48,  // TdhwSetUB/LB
    49,  // MaxTSetUB/LB  (spec: 49 = MaxTSetUBMaxTSetLB)
    13,  // FHBsize
};
inline uint16_t ot_slow_read_id(uint32_t cycle_i) {
  return kSlowReadIds[cycle_i % kSlowReadCount];
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

}  // namespace hcs
