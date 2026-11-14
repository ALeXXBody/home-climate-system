#pragma once
/**
 * Complete boiler-side OpenTherm ID registry (spec v2.2, ihormelnyk enum
 * order). One table drives the slow-read sweep, the ot_log names and the
 * published ot_map — so a boiler that answers an ID nobody expected still
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

struct HcsOtIdInfo {
  uint8_t id;
  const char* name;
  uint8_t sweep;  // HcsOtSweep
  uint8_t ftype;  // HcsOtType
};

static const HcsOtIdInfo kHcsOtIds[] = {
    {0, "Status", HCS_OT_FAST, HCS_OT_RAW},
    {1, "TSet", HCS_OT_SKIP, HCS_OT_F88},
    {2, "MConfig", HCS_OT_SKIP, HCS_OT_RAW},
    {3, "SConfig", HCS_OT_SWEEP, HCS_OT_RAW},
    {4, "Command", HCS_OT_SKIP, HCS_OT_RAW},
    {5, "ASF", HCS_OT_SWEEP, HCS_OT_RAW},
    {6, "RBPflags", HCS_OT_SWEEP, HCS_OT_RAW},
    {7, "Cooling", HCS_OT_SKIP, HCS_OT_F88},
    {8, "TsetCH2", HCS_OT_SKIP, HCS_OT_F88},
    {9, "TrOverride", HCS_OT_SWEEP, HCS_OT_F88},
    {10, "TSP", HCS_OT_SWEEP, HCS_OT_RAW},
    {11, "TSPentry", HCS_OT_SPECIAL, HCS_OT_RAW},
    {12, "FHBentry", HCS_OT_SPECIAL, HCS_OT_RAW},
    {13, "FHBsize", HCS_OT_SWEEP, HCS_OT_RAW},
    {14, "MaxRelMod", HCS_OT_SKIP, HCS_OT_F88},
    {15, "MaxCapMinMod", HCS_OT_SWEEP, HCS_OT_RAW},
    {16, "TrSet", HCS_OT_SWEEP, HCS_OT_F88},
    {17, "RelMod", HCS_OT_FAST, HCS_OT_F88},
    {18, "CHPressure", HCS_OT_SWEEP, HCS_OT_F88},
    {19, "DHWFlow", HCS_OT_SWEEP, HCS_OT_F88},
    {20, "DayTime", HCS_OT_SWEEP, HCS_OT_RAW},
    {21, "Date", HCS_OT_SWEEP, HCS_OT_RAW},
    {22, "Year", HCS_OT_SWEEP, HCS_OT_U16},
    {23, "TrSetCH2", HCS_OT_SWEEP, HCS_OT_F88},
    {24, "Tr", HCS_OT_SWEEP, HCS_OT_F88},
    {25, "Tboiler", HCS_OT_FAST, HCS_OT_F88},
    {26, "Tdhw", HCS_OT_FAST, HCS_OT_F88},
    {27, "Toutside", HCS_OT_FAST, HCS_OT_F88},
    {28, "Tret", HCS_OT_FAST, HCS_OT_F88},
    {29, "Tstorage", HCS_OT_SWEEP, HCS_OT_F88},
    {30, "Tcollector", HCS_OT_SWEEP, HCS_OT_F88},
    {31, "TflowCH2", HCS_OT_SWEEP, HCS_OT_F88},
    {32, "Tdhw2", HCS_OT_SWEEP, HCS_OT_F88},
    {33, "Texhaust", HCS_OT_SWEEP, HCS_OT_F88},
    // 34..47: reserved by the spec — never probed.
    {48, "TdhwSetUB", HCS_OT_SWEEP, HCS_OT_RAW},
    {49, "MaxTSetUB", HCS_OT_SWEEP, HCS_OT_RAW},
    {50, "HcratioUB", HCS_OT_SWEEP, HCS_OT_RAW},
    // 51..55: reserved by the spec — never probed.
    {56, "TdhwSet", HCS_OT_SWEEP, HCS_OT_F88},
    {57, "MaxTSet", HCS_OT_SWEEP, HCS_OT_F88},
    {58, "Hcratio", HCS_OT_SWEEP, HCS_OT_F88},
    // 59..99: reserved by the spec — never probed.
    {100, "RemOverride", HCS_OT_SWEEP, HCS_OT_RAW},
    // 101..114: reserved by the spec — never probed.
    {115, "OEMDiag", HCS_OT_SWEEP, HCS_OT_U16},
    {116, "BurnerStarts", HCS_OT_SWEEP, HCS_OT_U16},
    {117, "CHPumpStarts", HCS_OT_SWEEP, HCS_OT_U16},
    {118, "DHWPumpStarts", HCS_OT_SWEEP, HCS_OT_U16},
    {119, "DHWBurnerStarts", HCS_OT_SWEEP, HCS_OT_U16},
    {120, "BurnerHours", HCS_OT_SWEEP, HCS_OT_U16},
    {121, "CHPumpHours", HCS_OT_SWEEP, HCS_OT_U16},
    {122, "DHWPumpHours", HCS_OT_SWEEP, HCS_OT_U16},
    {123, "DHWBurnerHours", HCS_OT_SWEEP, HCS_OT_U16},
    // 124 OpenThermVersionMaster: our own version, nothing to ask the slave.
    {125, "OTVersionSlave", HCS_OT_SWEEP, HCS_OT_F88},
    // 126 MasterVersion: our own version, nothing to ask the slave.
    {127, "SlaveVersion", HCS_OT_SWEEP, HCS_OT_RAW},
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

/** Table name for an ID (ot_log + diagnostics). Never null. */
inline const char* hcs_ot_name(uint8_t id) {
  for (size_t i = 0; i < kHcsOtIdCount; i++) {
    if (kHcsOtIds[i].id == id) return kHcsOtIds[i].name;
  }
  return "?";
}
