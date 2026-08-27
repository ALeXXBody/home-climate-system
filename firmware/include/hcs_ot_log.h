#pragma once
/**
 * OpenTherm frame console — ring buffer of recent master↔boiler exchanges.
 *
 * MsgID names match ihormelnyk/OpenTherm Library + OT protocol table.
 * Unsupported IDs on a given boiler show as T/O or UNK — that is normal,
 * not a wiring fault.
 */

#include <Arduino.h>
#include <OpenTherm.h>

namespace hcs {

struct OtLogEntry {
  uint32_t t_ms;
  uint32_t req;
  uint32_t resp;
  uint8_t status;  // OpenThermResponseStatus
};

class OtLog {
 public:
  static constexpr uint8_t CAPACITY = 64;

  void record(uint32_t req, uint32_t resp, OpenThermResponseStatus st) {
    entries_[head_] = {millis(), req, resp, (uint8_t)st};
    head_ = (head_ + 1) % CAPACITY;
    if (count_ < CAPACITY) count_++;
#if defined(HCS_OT_LOG_SERIAL)
    char line[96];
    format(entryAt_(count_ - 1), line, sizeof(line));
    Serial.println(line);
#endif
  }

  uint8_t count() const { return count_; }

  const OtLogEntry& entryAt_(uint8_t newest_index) const {
    uint8_t start = (CAPACITY + head_ - count_) % CAPACITY;
    return entries_[(start + newest_index) % CAPACITY];
  }

  const OtLogEntry* entry(uint8_t newest_index) const {
    if (newest_index >= count_) return nullptr;
    return &entryAt_(newest_index);
  }

  void clear() {
    head_ = 0;
    count_ = 0;
  }

  static const char* typeName(uint8_t mt) {
    switch ((OpenThermMessageType)mt) {
      case OpenThermMessageType::READ_DATA:    return "READ";
      case OpenThermMessageType::WRITE_DATA:   return "WRITE";
      case OpenThermMessageType::INVALID_DATA: return "INVAL";
      case OpenThermMessageType::RESERVED:     return "RSV";
      case OpenThermMessageType::READ_ACK:     return "R-ACK";
      case OpenThermMessageType::WRITE_ACK:    return "W-ACK";
      case OpenThermMessageType::DATA_INVALID: return "D-INV";
      case OpenThermMessageType::UNKNOWN_DATA_ID: return "UNK-ID";
      default:                                 return "UNK";
    }
  }

  static const char* statusName(uint8_t st) {
    switch ((OpenThermResponseStatus)st) {
      case OpenThermResponseStatus::SUCCESS: return "OK";
      case OpenThermResponseStatus::INVALID: return "BAD";
      case OpenThermResponseStatus::TIMEOUT: return "T/O";
      case OpenThermResponseStatus::NONE:    return "-";
      default:                               return "?";
    }
  }

  /** Canonical OT MsgID short names (numeric IDs match the protocol). */
  static bool idName(uint8_t id, char* out, size_t len) {
    const char* n = nullptr;
    switch (id) {
      case 0:   n = "Status"; break;
      case 1:   n = "TSet"; break;
      case 2:   n = "MConfig"; break;
      case 3:   n = "SConfig"; break;
      case 4:   n = "Command"; break;
      case 5:   n = "ASFflags"; break;
      case 6:   n = "RBPflags"; break;
      case 7:   n = "Cooling"; break;
      case 8:   n = "TSetCH2"; break;
      case 9:   n = "TrOverride"; break;
      case 10:  n = "TSP"; break;
      case 11:  n = "TSPidx"; break;
      case 12:  n = "FHBsize"; break;
      case 13:  n = "FHBidx"; break;
      case 14:  n = "MaxRelMod"; break;   // MaxRelModLevelSetting
      case 15:  n = "MaxCapMin"; break;  // MaxCapacityMinModLevel
      case 16:  n = "TrSet"; break;      // room setpoint
      case 17:  n = "RelMod"; break;     // Relative Modulation Level
      case 18:  n = "CHPress"; break;    // CH pressure (bar)
      case 19:  n = "DHWFlow"; break;
      case 20:  n = "DayTime"; break;
      case 21:  n = "Date"; break;
      case 22:  n = "Year"; break;
      case 23:  n = "TrSetCH2"; break;
      case 24:  n = "Tr"; break;         // room temperature
      case 25:  n = "Tboiler"; break;    // flow water °C
      case 26:  n = "Tdhw"; break;
      case 27:  n = "Toutside"; break;   // outdoor °C (Tout)
      case 28:  n = "Tret"; break;       // return water °C
      case 29:  n = "Tstorage"; break;
      case 30:  n = "Tcollector"; break;
      case 31:  n = "TflowCH2"; break;
      case 32:  n = "Tdhw2"; break;
      case 33:  n = "Texhaust"; break;
      case 48:  n = "TdhwSetUB"; break;
      case 49:  n = "MaxTSetUB"; break;
      case 50:  n = "HcratioUB"; break;
      case 56:  n = "TdhwSet"; break;
      case 57:  n = "MaxTSet"; break;
      case 58:  n = "Hcratio"; break;
      case 100: n = "RemOverride"; break;
      case 115: n = "OEMDiag"; break;
      case 116: n = "BurnerStarts"; break;
      case 117: n = "CHPumpStarts"; break;
      case 118: n = "DHWPumpStarts"; break;
      case 119: n = "DHWBurnStarts"; break;
      case 120: n = "BurnerHours"; break;
      case 121: n = "CHPumpHours"; break;
      case 122: n = "DHWPumpHours"; break;
      case 123: n = "DHWBurnHours"; break;
      case 124: n = "OTVerMaster"; break;
      case 125: n = "OTVerSlave"; break;
      case 126: n = "MasterVer"; break;
      case 127: n = "SlaveVer"; break;
      default:  n = nullptr; break;
    }
    if (!n) return false;
    strlcpy(out, n, len);
    return true;
  }

  static void format(const OtLogEntry& e, char* out, size_t len) {
    char rid[16], sid[16];
    uint8_t r_id = (uint8_t)OpenTherm::getDataID(e.req);
    uint8_t s_id = (uint8_t)OpenTherm::getDataID(e.resp);
    if (!idName(r_id, rid, sizeof(rid))) snprintf(rid, sizeof(rid), "id%u", r_id);
    if (!idName(s_id, sid, sizeof(sid))) snprintf(sid, sizeof(sid), "id%u", s_id);

    char payload[28];
    formatPayload(e.resp, s_id, e.status, payload, sizeof(payload));

    // Note: UNK-ID / T/O on optional sensors (Toutside/Tret) is normal when
    // the boiler has no outdoor/return probe — use 1-Wire roles instead.
    //
    // Conversions and args pair one-to-one. v1.4.1–1.4.10 shipped a
    // misordered pair (sid ↔ resp-hex) that fed an INTEGER to %s: snprintf
    // read a "string" from a frame-derived address and load-faulted →
    // PANIC on every /api/otlog request once real OT frames existed.
    snprintf(out, len, "[%6lu.%02lus] %-5s %-9s %04X -> %s %-5s %-9s %04X | %s",
             (unsigned long)(e.t_ms / 1000),
             (unsigned long)((e.t_ms % 1000) / 10),
             typeName((uint8_t)OpenTherm::getMessageType(e.req)),
             rid,
             (unsigned)(e.req & 0xFFFF),
             statusName(e.status),
             typeName((uint8_t)OpenTherm::getMessageType(e.resp)),
             sid,
             (unsigned)(e.resp & 0xFFFF),
             payload);
  }

 private:
  static void formatPayload(uint32_t frame, uint8_t id, uint8_t status,
                            char* out, size_t len) {
    if (status != (uint8_t)OpenThermResponseStatus::SUCCESS) {
      snprintf(out, len, "(no data)");
      return;
    }
    // Flag / dual-byte IDs: print hi/lo hex rather than a bogus °C float.
    switch (id) {
      case 0:   // Status
      case 2: case 3: case 5: case 6:
      case 15:  // max capacity / min mod
      case 48: case 49: case 50:
        snprintf(out, len, "hi=%u lo=%u",
                 (unsigned)((frame >> 8) & 0xFF),
                 (unsigned)(frame & 0xFF));
        return;
      case 115: case 116: case 117: case 118: case 119:
      case 120: case 121: case 122: case 123:
        snprintf(out, len, "= %u", (unsigned)OpenTherm::getUInt(frame));
        return;
      default:
        break;
    }
    float f = OpenTherm::getFloat(frame);
    if (f > -80.0f && f < 160.0f) {
      snprintf(out, len, "= %.2f", (double)f);
    } else {
      snprintf(out, len, "= %u", (unsigned)OpenTherm::getUInt(frame));
    }
  }

  OtLogEntry entries_[CAPACITY] = {};
  uint8_t head_ = 0;
  uint8_t count_ = 0;
};

}  // namespace hcs
