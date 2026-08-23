#pragma once
/**
 * Boiler diagnostics -> clean text (portable, host-testable).
 *
 * Sources (OpenTherm spec v2.2+):
 *   MsgID 5    ASF flags — application-specific fault bits
 *   MsgID 115  OEM diagnostic code — brand-specific number (u16)
 *
 * The ASF bit labels follow the public spec; OEM codes are vendor-specific,
 * so we surface them verbatim ("diagnostic code N") rather than guessing.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

namespace hcs {

struct BoilerDiag {
  bool valid_asf = false;      // seen a valid ASF read this session
  bool valid_oem = false;
  uint8_t asf = 0;             // MsgID 5 flag byte
  uint16_t oem = 0;            // MsgID 6 value
  unsigned long age_ms = 0;    // millis() age of last update (0 = fresh)
};

/** True when any fault indication is present. */
inline bool boiler_has_fault(const BoilerDiag& d) {
  return (d.valid_asf && d.asf != 0) || (d.valid_oem && d.oem != 0);
}

/**
 * Render a one-line human-readable summary into out (NUL-terminated).
 * Returns the number of faults mentioned (ASF bits + nonzero OEM code).
 */
inline int boiler_diag_text(const BoilerDiag& d, char* out, size_t n) {
  if (!out || n == 0) return 0;
  size_t p = 0;
  int faults = 0;

  auto put = [&](const char* s) {
    while (*s && p + 1 < n) out[p++] = *s++;
  };
  auto sep = [&](bool first) {
    if (!first) put("; ");
  };

  if (d.valid_asf) {
    static const struct { uint8_t bit; const char* label; } kBits[] = {
        {0, "service request"},
        {1, "lockout - reset required"},
        {2, "low water pressure"},
        {3, "gas or flame fault"},
        {4, "air temperature sensor fault"},
        {5, "CH2 service request"},
        {6, "CH2 lockout - reset required"},
        {7, "CH2 gas or flame fault"},
    };
    bool first = true;
    for (auto& b : kBits) {
      if (d.asf & (1 << b.bit)) {
        sep(first);
        first = false;
        put(b.label);
        faults++;
      }
    }
    if (d.asf == 0) {
      // healthy ASF contributes no text
    }
  }

  if (d.valid_oem && d.oem != 0) {
    sep(faults == 0);
    char num[40];
    snprintf(num, sizeof(num), "boiler diagnostic code %u", (unsigned)d.oem);
    put(num);
    faults++;
  }

  if (faults == 0) {
    put((d.valid_asf || d.valid_oem) ? "no faults" : "no data");
  }
  out[p] = '\0';
  return faults;
}

/** Short state token for HA sensors / badges. */
inline const char* boiler_diag_state(const BoilerDiag& d) {
  if (!(d.valid_asf || d.valid_oem)) return "unknown";
  return boiler_has_fault(d) ? "fault" : "ok";
}

}  // namespace hcs
