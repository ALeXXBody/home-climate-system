#pragma once
/**
 * Portable sensor-routing logic for Home Climate System (host-testable).
 *
 * 1-Wire (DS18B20) probes can backfill values some boilers don't report
 * (outdoor temp MsgID 27, return temp MsgID 25). This module owns:
 *   - DS18B20 address <-> hex-string codec (settings storage format)
 *   - role assignment validation
 *   - the override rule: a valid, fresh sensor reading wins over the
 *     OpenTherm-provided value; otherwise the OT value passes through.
 *
 * Free of Arduino types so `pio test -e native` exercises the exact code.
 */

#include <stdint.h>
#include <stddef.h>

namespace hcs {

/** Fixed DS18B20 ROM address length. */
constexpr size_t kOwAddrBytes = 8;
/** Hex chars needed to store one address (no NUL). */
constexpr size_t kOwAddrHexLen = kOwAddrBytes * 2;

enum OwRole : uint8_t {
  OW_ROLE_NONE = 0,
  OW_ROLE_OUTDOOR = 1,
  OW_ROLE_RETURN = 2,
};

/** Encode 8 address bytes into 16 uppercase hex chars + NUL (17 bytes). */
inline void ow_addr_to_hex(const uint8_t* addr, char* out /*len>=17*/) {
  static const char* kHex = "0123456789ABCDEF";
  for (size_t i = 0; i < kOwAddrBytes; i++) {
    out[i * 2] = kHex[(addr[i] >> 4) & 0xF];
    out[i * 2 + 1] = kHex[addr[i] & 0xF];
  }
  out[kOwAddrHexLen] = '\0';
}

/** Parse exactly 16 hex chars into 8 bytes. Returns false on bad input. */
inline bool ow_hex_to_addr(const char* hex, uint8_t* out /*len>=8*/) {
  if (!hex || !out) return false;
  size_t n = 0;
  while (hex[n] != '\0') n++;
  if (n != kOwAddrHexLen) return false;
  for (size_t i = 0; i < kOwAddrBytes; i++) {
    uint8_t hi = 0, lo = 0;
    char c = hex[i * 2], d = hex[i * 2 + 1];
    // case-insensitive nibble decode; reject anything else
    if (c >= '0' && c <= '9') hi = c - '0';
    else if (c >= 'A' && c <= 'F') hi = c - 'A' + 10;
    else if (c >= 'a' && c <= 'f') hi = c - 'a' + 10;
    else return false;
    if (d >= '0' && d <= '9') lo = d - '0';
    else if (d >= 'A' && d <= 'F') lo = d - 'A' + 10;
    else if (d >= 'a' && d <= 'f') lo = d - 'a' + 10;
    else return false;
    out[i] = (uint8_t)((hi << 4) | lo);
  }
  return true;
}

/** Validate a role name from JSON/API input. */
inline bool ow_role_from_name(const char* name, OwRole* out) {
  if (!name || !out) return false;
  if (name[0] == 'n' || name[0] == 'N') {  // none
    *out = OW_ROLE_NONE;
    return true;
  }
  // unambiguous prefixes: out(door), ret(urn)
  if (name[0] == 'o' || name[0] == 'O') {
    if (name[1] == 'u' || name[1] == 'U') {
      *out = OW_ROLE_OUTDOOR;
      return true;
    }
    return false;
  }
  if (name[0] == 'r' || name[0] == 'R') {
    if (name[1] == 'e' || name[1] == 'E') {
      *out = OW_ROLE_RETURN;
      return true;
    }
    return false;
  }
  return false;
}

/** True when the reading exists and is younger than max_age_ms. */
inline bool ow_reading_fresh(bool valid, unsigned long age_ms,
                             unsigned long max_age_ms) {
  return valid && age_ms <= max_age_ms;
}

struct TempValue {
  bool valid = false;
  float celsius = 0.0f;
};

/**
 * Override rule. Sensor beats OpenTherm only when the sensor side is
 * valid+fresh; otherwise whatever the bus reported passes through
 * unchanged. `sensor_is_authoritative` marks that a probe is ASSIGNED to
 * this role (user intent), independent of its current health.
 */
inline TempValue resolve_temp(bool sensor_assigned, bool sensor_fresh,
                              float sensor_c, bool ot_valid, float ot_c) {
  TempValue v;
  if (sensor_assigned && sensor_fresh) {
    v.valid = true;
    v.celsius = sensor_c;
    return v;
  }
  v.valid = ot_valid;
  v.celsius = ot_c;
  return v;
}

}  // namespace hcs
