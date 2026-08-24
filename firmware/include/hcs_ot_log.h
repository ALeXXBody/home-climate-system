#pragma once
/**
 * OpenTherm frame console — a small ring buffer of recent master↔boiler
 * exchanges, for debugging the bus from the board's web UI.
 *
 * Every request/response pair that passes through OtMaster is recorded:
 *   timestamp · request type+ID+payload · response type+ID+payload · status
 * The console renders them as plain lines; nothing here talks to Home
 * Assistant or MQTT — this is purely an on-device service tool.
 *
 * Memory: 64 entries × 16 B = 1 KB RAM (fine on ESP8266).
 * Serial echo of each frame is opt-in: build with -DHCS_OT_LOG_SERIAL.
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
    // newest_index: 0 = oldest retained … count()-1 = newest
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

  // ---- human-readable helpers -------------------------------------------
  static const __FlashStringHelper* typeName(uint8_t mt) {
    switch ((OpenThermMessageType)mt) {
      case OpenThermMessageType::READ_DATA:   return F("READ");
      case OpenThermMessageType::WRITE_DATA:  return F("WRITE");
      case OpenThermMessageType::INVALID_DATA:return F("INVAL");
      case OpenThermMessageType::RESERVED:    return F("RSV");
      case OpenThermMessageType::READ_ACK:    return F("R-ACK");
      case OpenThermMessageType::WRITE_ACK:   return F("W-ACK");
      case OpenThermMessageType::DATA_INVALID:return F("D-INV");
      default:                                return F("UNK");
    }
  }

  static const __FlashStringHelper* statusName(uint8_t st) {
    switch ((OpenThermResponseStatus)st) {
      case OpenThermResponseStatus::SUCCESS: return F("OK");
      case OpenThermResponseStatus::INVALID: return F("BAD");
      case OpenThermResponseStatus::TIMEOUT: return F("T/O");
      case OpenThermResponseStatus::NONE:    return F("-");
      default:                               return F("?");
    }
  }

  // Short names for the message IDs worth recognising at a glance.
  static bool idName(uint8_t id, char* out, size_t len) {
    const char* n = nullptr;
    switch (id) {
      case 0:   n = "Status"; break;
      case 1:   n = "TSet"; break;
      case 2:   n = "MConfigMA"; break;
      case 3:   n = "SConfigMA"; break;
      case 4:   n = "CmdCode"; break;
      case 5:   n = "ASFflags"; break;
      case 6:   n = "RBPflags"; break;
      case 8:   n = "TSetCH2"; break;
      case 9:   n = "TrOverride"; break;
      case 12:  n = "TdhwSetUB"; break;
      case 14:  n = "MaxRelModLevel"; break;
      case 15:  n = "TBoilerSet"; break;
      case 16:  n = "TBoiler"; break;
      case 17:  n = "Tdhw"; break;
      case 18:  n = "Tout"; break;
      case 19:  n = "TRet"; break;
      case 23:  n = "Twater"; break;
      case 24:  n = "TdhwUB"; break;
      case 25:  n = "TboilerUB"; break;
      case 26:  n = "Texhaust"; break;
      case 27:  n = "Toutside"; break;
      case 28:  n = "TretMax"; break;
      case 48:  n = "TdhwSet"; break;
      case 49:  n = "MaxTSet"; break;
      case 56:  n = "TdhwSet2"; break;
      case 57:  n = "MaxTSet2"; break;
      case 100: n = "RelModLevel"; break;
      case 101: n = "CHPressure"; break;
      case 102: n = "DHWFlowRate"; break;
      case 115: n = "OEEMeanwhile"; break;
      case 116: n = "OEMViolation"; break;
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

    char payload[24];
    formatPayload(e.resp, s_id, payload, sizeof(payload));

    snprintf(out, len, "[%6lu.%02lus] %-5s %-9s %04X -> %s %-9s %04X %s | %s",
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
  // f8.8 payloads print as °C-style floats; everything else as u16.
  static void formatPayload(uint32_t frame, uint8_t /*id*/, char* out, size_t len) {
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
