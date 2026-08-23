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
  OW_ROLE_CUSTOM = 3,
};

/** Longest user-visible custom sensor name (MQTT leaf under x/). */
constexpr size_t kOwNameMax = 16;
/** Maximum probes tracked simultaneously. */
constexpr size_t kOwMaxSlots = 8;

/**
 * Per-probe assignment persisted in settings. addr is the 16-char hex ROM
 * id; role is an OwRole; name only meaningful for OW_ROLE_CUSTOM.
 */
struct OwSlot {
  char addr[kOwAddrHexLen + 1] = {0};
  uint8_t role = OW_ROLE_NONE;
  char name[kOwNameMax + 1] = {0};
};

/**
 * Sensor health verdict. Detection validates a probe at every read:
 *   DISCONNECTED  no presence pulse / bus reports device gone
 *   CRC           scratchpad failed its CRC8 check
 *   IMPLAUSIBLE   reading outside the physically usable window
 *   STUCK85       repeated exact +85.0 readings — the DS18B20 power-on
 *                 register value; classic symptom of parasite-power or
 *                 conversion-never-completes wiring faults
 *   UNSTABLE      impossible step change vs previous good reading
 */
enum OwHealth : uint8_t {
  OW_HEALTH_UNKNOWN = 0,
  OW_HEALTH_OK,
  OW_HEALTH_DISCONNECTED,
  OW_HEALTH_CRC,
  OW_HEALTH_IMPLAUSIBLE,
  OW_HEALTH_STUCK85,
  OW_HEALTH_UNSTABLE,
  OW_HEALTH_UNSUPPORTED,  // 1-Wire family code we cannot read
};

/** Short stable identifier for JSON/API/portal display. */
inline const char* ow_health_name(OwHealth h) {
  switch (h) {
    case OW_HEALTH_OK: return "ok";
    case OW_HEALTH_DISCONNECTED: return "disconnected";
    case OW_HEALTH_CRC: return "crc";
    case OW_HEALTH_IMPLAUSIBLE: return "implausible";
    case OW_HEALTH_STUCK85: return "stuck85";
    case OW_HEALTH_UNSTABLE: return "unstable";
    case OW_HEALTH_UNSUPPORTED: return "unsupported";
    default: return "unknown";
  }
}

/**
 * Classify one raw reading against the previous GOOD one.
 * raw == DEVICE_DISCONNECTED sentinel (-127) maps to DISCONNECTED.
 */
inline OwHealth ow_classify(float raw, bool have_prev_good, float prev_good) {
  if (raw <= -100.0f) return OW_HEALTH_DISCONNECTED;
  if (raw <= -55.0f || raw >= 125.0f) return OW_HEALTH_IMPLAUSIBLE;
  if (have_prev_good) {
    // Exact repeat of the power-on default twice => converter never ran.
    if (prev_good == 85.0f && raw == 85.0f) return OW_HEALTH_STUCK85;
    // Air cannot move 15 °C between polls; that is electrical noise.
    float d = raw - prev_good;
    if (d < -15.0f || d > 15.0f) return OW_HEALTH_UNSTABLE;
  }
  return OW_HEALTH_OK;
}

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

/** Role name for JSON/API/portal (stable lowercase identifier). */
inline const char* ow_role_name(OwRole r) {
  switch (r) {
    case OW_ROLE_OUTDOOR: return "outdoor";
    case OW_ROLE_RETURN: return "return";
    case OW_ROLE_CUSTOM: return "custom";
    default: return "none";
  }
}

/** Validate a role name from JSON/API input. */
inline bool ow_role_from_name(const char* name, OwRole* out) {
  if (!name || !out) return false;
  if (name[0] == 'n' || name[0] == 'N') {  // none
    *out = OW_ROLE_NONE;
    return true;
  }
  // unambiguous prefixes: out(door), ret(urn), cus(tom)/x
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
  if (name[0] == 'c' || name[0] == 'C') {
    *out = OW_ROLE_CUSTOM;
    return true;
  }
  if (name[0] == 'x' || name[0] == 'X') {
    *out = OW_ROLE_CUSTOM;
    return true;
  }
  return false;
}

/**
 * Sanitize a custom sensor name into a safe MQTT leaf / HA entity slug.
 * Accepts [A-Za-z0-9_-], lowercases, collapses other chars to '_', trims
 * leading/trailing underscores. Requires 2..kOwNameMax chars after sanitize
 * and a leading letter or underscore. Returns false on reject (out cleared).
 */
inline bool ow_sanitize_name(const char* in, char* out, size_t out_len) {
  if (!out || out_len < 3) return false;
  out[0] = '\0';
  if (!in) return false;
  size_t j = 0;
  bool last_us = false;
  for (size_t i = 0; in[i] != '\0' && j + 1 < out_len && j < kOwNameMax; i++) {
    char c = in[i];
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    if (!ok) {
      if (c == '-' || c == ' ' || c == '.') c = '_';
      else continue;
    }
    if (c == '_') {
      if (j == 0 || last_us) continue;
      last_us = true;
    } else {
      last_us = false;
    }
    out[j++] = c;
  }
  while (j > 0 && out[j - 1] == '_') j--;
  out[j] = '\0';
  if (j < 2) { out[0] = '\0'; return false; }
  if (!(out[0] >= 'a' && out[0] <= 'z') && out[0] != '_') {
    out[0] = '\0';
    return false;
  }
  return true;
}

/** Case-insensitive compare of two 16-char hex addresses (no length check). */
inline bool ow_addr_eq(const char* a, const char* b) {
  if (!a || !b) return false;
  for (size_t i = 0; i < kOwAddrHexLen; i++) {
    char ca = a[i], cb = b[i];
    if (ca >= 'a' && ca <= 'f') ca = (char)(ca - 'a' + 'A');
    if (cb >= 'a' && cb <= 'f') cb = (char)(cb - 'a' + 'A');
    if (ca != cb) return false;
  }
  return a[kOwAddrHexLen] == '\0' && b[kOwAddrHexLen] == '\0';
}

/** Index of slot holding addr, or -1. */
inline int ow_slot_find(const OwSlot* slots, size_t n, const char* addr_hex) {
  if (!slots || !addr_hex) return -1;
  for (size_t i = 0; i < n; i++) {
    if (slots[i].addr[0] && ow_addr_eq(slots[i].addr, addr_hex)) return (int)i;
  }
  return -1;
}

/** First free slot index, or -1. */
inline int ow_slot_free(const OwSlot* slots, size_t n) {
  if (!slots) return -1;
  for (size_t i = 0; i < n; i++) {
    if (slots[i].addr[0] == '\0') return (int)i;
  }
  return -1;
}

/**
 * Assign a role to a probe. Rules:
 *   - outdoor/return are unique channels (steal from any other slot)
 *   - custom requires a sanitized non-empty name unique among customs
 *   - none clears the slot for this address
 * Returns false on bad addr/name, full table, or name collision.
 */
inline bool ow_assign(OwSlot* slots, size_t n, const char* addr_hex,
                      OwRole role, const char* name_in) {
  if (!slots || !addr_hex) return false;
  uint8_t raw[kOwAddrBytes];
  if (!ow_hex_to_addr(addr_hex, raw)) return false;
  char hex[kOwAddrHexLen + 1];
  ow_addr_to_hex(raw, hex);  // canonical uppercase

  char name[kOwNameMax + 1] = {0};
  if (role == OW_ROLE_CUSTOM) {
    if (!ow_sanitize_name(name_in, name, sizeof(name))) return false;
    // unique among other customs
    for (size_t i = 0; i < n; i++) {
      if (slots[i].role != OW_ROLE_CUSTOM) continue;
      if (ow_addr_eq(slots[i].addr, hex)) continue;
      // case-insensitive name compare (already lowercased)
      size_t k = 0;
      while (slots[i].name[k] && name[k] && slots[i].name[k] == name[k]) k++;
      if (slots[i].name[k] == '\0' && name[k] == '\0') return false;
    }
  }

  // unique channel roles: clear from other slots
  if (role == OW_ROLE_OUTDOOR || role == OW_ROLE_RETURN) {
    for (size_t i = 0; i < n; i++) {
      if (slots[i].role == (uint8_t)role && !ow_addr_eq(slots[i].addr, hex)) {
        slots[i].role = OW_ROLE_NONE;
        slots[i].name[0] = '\0';
        // keep addr so reassignment is easy; empty role = unassigned
      }
    }
  }

  int idx = ow_slot_find(slots, n, hex);
  if (role == OW_ROLE_NONE) {
    if (idx >= 0) {
      slots[idx].addr[0] = '\0';
      slots[idx].role = OW_ROLE_NONE;
      slots[idx].name[0] = '\0';
    }
    return true;
  }
  if (idx < 0) {
    idx = ow_slot_free(slots, n);
    if (idx < 0) return false;
  }
  // copy hex
  for (size_t i = 0; i <= kOwAddrHexLen; i++) slots[idx].addr[i] = hex[i];
  slots[idx].role = (uint8_t)role;
  if (role == OW_ROLE_CUSTOM) {
    for (size_t i = 0; i <= kOwNameMax; i++) {
      slots[idx].name[i] = name[i];
      if (!name[i]) break;
    }
  } else {
    slots[idx].name[0] = '\0';
  }
  return true;
}

/** Build slots from legacy outdoor/return hex strings (settings migration). */
inline uint8_t ow_slots_from_legacy(const char* outdoor_hex,
                                    const char* return_hex,
                                    OwSlot* slots, size_t n) {
  if (!slots || n == 0) return 0;
  for (size_t i = 0; i < n; i++) {
    slots[i].addr[0] = '\0';
    slots[i].role = OW_ROLE_NONE;
    slots[i].name[0] = '\0';
  }
  uint8_t used = 0;
  if (outdoor_hex && outdoor_hex[0]) {
    if (ow_assign(slots, n, outdoor_hex, OW_ROLE_OUTDOOR, nullptr)) used++;
  }
  if (return_hex && return_hex[0]) {
    if (ow_assign(slots, n, return_hex, OW_ROLE_RETURN, nullptr)) used++;
  }
  return used;
}

/** Lookup role for an address; returns NONE if unassigned. */
inline OwRole ow_role_of(const OwSlot* slots, size_t n, const char* addr_hex) {
  int i = ow_slot_find(slots, n, addr_hex);
  if (i < 0) return OW_ROLE_NONE;
  return (OwRole)slots[i].role;
}

/** Address currently holding a channel role (outdoor/return), or empty. */
inline const char* ow_addr_for_role(const OwSlot* slots, size_t n, OwRole role) {
  if (!slots) return "";
  for (size_t i = 0; i < n; i++) {
    if (slots[i].addr[0] && slots[i].role == (uint8_t)role) return slots[i].addr;
  }
  return "";
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
