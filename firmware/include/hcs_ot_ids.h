#pragma once
/**
 * Complete boiler-side OpenTherm ID registry (spec v2.2, ihormelnyk enum
 * order). One table drives the slow-read sweep and the published
 * ot_map — so a boiler that answers an ID nobody expected still
 * shows up instead of being silently ignored.
 *
 * Portable C/C++ (no Arduino): host-testable with `pio test -e native`.
 *
 * Sweep policy: READ-only for everything readable. WRITE-only IDs (1, 2,
 * 4, 7, 8, 14) are skipped — the wire writes stay exactly the explicit
 * command paths they are today. Indexed IDs (11 TSP, 12 FHB) need
 * index-writes per entry and stay on their dedicated paths.
 */

#include <stdint.h>
#include <stddef.h>

enum HcsOtSweep : uint8_t {
  HCS_OT_SWEEP = 0,  // read in the slow rotation, stored in ot_map
  HCS_OT_FAST = 1,   // handled every cycle on a dedicated path (skip here)
  HCS_OT_SKIP = 2,   // write-only or otherwise excluded from the sweep
  HCS_OT_SPECIAL = 3,  // indexed access, dedicated path (TSP/FHB entries)
};

enum HcsOtType : uint8_t {
  HCS_OT_F88 = 0,  // signed fixed point, value / 256.0 (covers s16 too)
  HCS_OT_U16 = 1,  // unsigned counter / version, published as integer
  HCS_OT_RAW = 2,  // flags, pairs, packed date/time — raw u16, hub decodes
};

// NOTE: no display names here on purpose — the esp32_d1_mini_gw image
// lives 544 B under its OTA slot, and 52 name strings cost ~680 B of
// flash. Display names live in the hub panel (OT_NAMES) and the ot_log
// decoder, which already covers every polled ID. IDs + decode types are
// the firmware's whole job.
struct HcsOtIdInfo {
  uint8_t id;
  uint8_t sweep;  // HcsOtSweep
  uint8_t ftype;  // HcsOtType
};

static const HcsOtIdInfo kHcsOtIds[] = {
    {0, HCS_OT_FAST, HCS_OT_RAW},
    {1, HCS_OT_SKIP, HCS_OT_F88},
    {2, HCS_OT_SKIP, HCS_OT_RAW},
    {3, HCS_OT_SWEEP, HCS_OT_RAW},
    {4, HCS_OT_SKIP, HCS_OT_RAW},
    {5, HCS_OT_SWEEP, HCS_OT_RAW},
    {6, HCS_OT_SWEEP, HCS_OT_RAW},
    {7, HCS_OT_SKIP, HCS_OT_F88},
    {8, HCS_OT_SKIP, HCS_OT_F88},
    {9, HCS_OT_SWEEP, HCS_OT_F88},
    {10, HCS_OT_SWEEP, HCS_OT_RAW},
    {11, HCS_OT_SPECIAL, HCS_OT_RAW},
    {12, HCS_OT_SPECIAL, HCS_OT_RAW},
    {13, HCS_OT_SWEEP, HCS_OT_RAW},
    {14, HCS_OT_SKIP, HCS_OT_F88},
    {15, HCS_OT_SWEEP, HCS_OT_RAW},
    {16, HCS_OT_SWEEP, HCS_OT_F88},
    {17, HCS_OT_FAST, HCS_OT_F88},
    {18, HCS_OT_SWEEP, HCS_OT_F88},
    {19, HCS_OT_SWEEP, HCS_OT_F88},
    {20, HCS_OT_SWEEP, HCS_OT_RAW},
    {21, HCS_OT_SWEEP, HCS_OT_RAW},
    {22, HCS_OT_SWEEP, HCS_OT_U16},
    {23, HCS_OT_SWEEP, HCS_OT_F88},
    {24, HCS_OT_SWEEP, HCS_OT_F88},
    {25, HCS_OT_FAST, HCS_OT_F88},
    {26, HCS_OT_FAST, HCS_OT_F88},
    {27, HCS_OT_FAST, HCS_OT_F88},
    {28, HCS_OT_FAST, HCS_OT_F88},
    {29, HCS_OT_SWEEP, HCS_OT_F88},
    {30, HCS_OT_SWEEP, HCS_OT_F88},
    {31, HCS_OT_SWEEP, HCS_OT_F88},
    {32, HCS_OT_SWEEP, HCS_OT_F88},
    {33, HCS_OT_SWEEP, HCS_OT_F88},
    // 34..47: reserved by the spec — never probed.
    {48, HCS_OT_SWEEP, HCS_OT_RAW},
    {49, HCS_OT_SWEEP, HCS_OT_RAW},
    {50, HCS_OT_SWEEP, HCS_OT_RAW},
    // 51..55: reserved by the spec — never probed.
    {56, HCS_OT_SWEEP, HCS_OT_F88},
    {57, HCS_OT_SWEEP, HCS_OT_F88},
    {58, HCS_OT_SWEEP, HCS_OT_F88},
    // 59..99: reserved by the spec — never probed.
    {100, HCS_OT_SWEEP, HCS_OT_RAW},
    // 101..114: reserved by the spec — never probed.
    {115, HCS_OT_SWEEP, HCS_OT_U16},
    {116, HCS_OT_SWEEP, HCS_OT_U16},
    {117, HCS_OT_SWEEP, HCS_OT_U16},
    {118, HCS_OT_SWEEP, HCS_OT_U16},
    {119, HCS_OT_SWEEP, HCS_OT_U16},
    {120, HCS_OT_SWEEP, HCS_OT_U16},
    {121, HCS_OT_SWEEP, HCS_OT_U16},
    {122, HCS_OT_SWEEP, HCS_OT_U16},
    {123, HCS_OT_SWEEP, HCS_OT_U16},
    // 124 OpenThermVersionMaster: our own version, nothing to ask the slave.
    {125, HCS_OT_SWEEP, HCS_OT_F88},
    // 126 MasterVersion: our own version, nothing to ask the slave.
    {127, HCS_OT_SWEEP, HCS_OT_RAW},
};

static const size_t kHcsOtIdCount =
    sizeof(kHcsOtIds) / sizeof(kHcsOtIds[0]);

/** Decode one raw u16 payload by table type. Returns false for unknown IDs
 *  (caller must not store those — only a READ_ACKed ID ever reaches here,
 *  but a belt-and-braces lookup keeps a corrupt table from inventing data).
 *  out is float for F88, integer-valued float for U16/RAW. */
inline bool hcs_ot_decode(uint8_t id, uint16_t raw, float& out) {
  for (size_t i = 0; i < kHcsOtIdCount; i++) {
    if (kHcsOtIds[i].id != id) continue;
    if (kHcsOtIds[i].sweep == HCS_OT_SKIP ||
        kHcsOtIds[i].sweep == HCS_OT_SPECIAL)
      return false;
    if (kHcsOtIds[i].ftype == HCS_OT_F88) {
      out = (float)((int16_t)raw) / 256.0f;
      return true;
    }
    out = (float)raw;
    return true;
  }
  return false;
}


