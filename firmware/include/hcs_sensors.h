#pragma once
// DS18B20 1-Wire manager for Home Climate System.
// Arduino builds only; portable rules live in hcs_sensor_logic.h.

#ifdef ARDUINO

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <memory>
#include <string.h>
#include "hcs_sensor_logic.h"

#ifndef HCS_ONEWIRE_PIN
#define HCS_ONEWIRE_PIN (-1)  // per-env override in platformio.ini; -1 = absent
#endif

namespace hcs {

constexpr unsigned long kOwPollMs = 15000;      // conversion request cadence
constexpr unsigned long kOwConvMs = 800;        // 12-bit conversion time
constexpr unsigned long kOwStaleMs = 90000;     // reading older than this = stale

struct OwDevice {
  uint8_t addr[kOwAddrBytes] = {0};
  bool present = false;     // last bus presence
  bool valid = false;       // last reading classified OK
  OwHealth health = OW_HEALTH_UNKNOWN;
  float celsius = NAN;
  float last_good_c = NAN;
  bool have_good = false;
  unsigned long ts_ms = 0;  // millis() of last successful OK read
  uint8_t family = 0;
};

class HcsSensors {
 public:
  /** Wire roles from settings slot table. */
  void configure(bool enabled, const OwSlot* slots, size_t n) {
    enabled_ = enabled;
    for (size_t i = 0; i < kOwMaxSlots; i++) slots_[i] = OwSlot{};
    size_t copy = n < kOwMaxSlots ? n : kOwMaxSlots;
    if (slots) {
      for (size_t i = 0; i < copy; i++) slots_[i] = slots[i];
    }
    n_slots_ = kOwMaxSlots;
  }

  void begin() {
    if (HCS_ONEWIRE_PIN < 0) return;
    pin_ = HCS_ONEWIRE_PIN;
    wire_.reset(new OneWire(pin_));
    bus_.reset(new DallasTemperature(wire_.get()));
    bus_->setWaitForConversion(false);  // non-blocking pattern
    scan();
    // Boot self-test: one blocking conversion so health is known immediately.
    if (count_) {
      bus_->requestTemperatures();
      delay(kOwConvMs);
      readAll();
    }
  }

  /** Call frequently from loop(); drives scan/convert/read state machine. */
  void loop() {
    if (!bus_) return;
    unsigned long now = millis();
    switch (phase_) {
      case IDLE:
        if (now - last_req_ms_ >= kOwPollMs) request();
        break;
      case CONVERTING:
        if (now - req_ms_ >= kOwConvMs) {
          readAll();
          phase_ = IDLE;
        }
        break;
    }
  }

  /**
   * Force a full re-scan + double conversion self-test (blocking ~2 s).
   * Use from the portal "Test sensors" button.
   */
  void selftestAll() {
    if (!bus_) return;
    scan();
    if (!count_) return;
    // first conversion
    bus_->requestTemperatures();
    delay(kOwConvMs);
    readAll();
    // second conversion — stuck85 / unstable need a previous good
    bus_->requestTemperatures();
    delay(kOwConvMs);
    readAll();
    phase_ = IDLE;
    last_req_ms_ = millis();
  }

  /** Re-enumerate devices on the bus (also runs at begin()). */
  void scan() {
    if (!bus_) return;
    count_ = 0;
    wire_->reset_search();
    uint8_t addr[kOwAddrBytes];
    while (count_ < kOwMaxSlots && wire_->search(addr)) {
      // OneWire::search only returns CRC-valid ROMs.
      OwDevice& d = devices_[count_];
      memcpy(d.addr, addr, kOwAddrBytes);
      d.family = addr[0];
      d.present = true;
      d.valid = false;
      d.celsius = NAN;
      d.ts_ms = 0;
      d.have_good = false;
      d.last_good_c = NAN;
      if (d.family != 0x28) {
        // DS18B20 family only — others are detected but not read.
        d.health = OW_HEALTH_UNSUPPORTED;
      } else {
        d.health = OW_HEALTH_UNKNOWN;
      }
      count_++;
    }
    // devices that vanished since last scan: leave them out of the list
  }

  bool enabled() const { return enabled_; }
  int pin() const { return pin_; }
  size_t count() const { return count_; }
  const OwDevice& device(size_t i) const { return devices_[i]; }
  const OwSlot* slots() const { return slots_; }
  size_t slotCount() const { return n_slots_; }

  bool outdoorAssigned() const {
    return ow_addr_for_role(slots_, n_slots_, OW_ROLE_OUTDOOR)[0] != '\0';
  }
  bool returnAssigned() const {
    return ow_addr_for_role(slots_, n_slots_, OW_ROLE_RETURN)[0] != '\0';
  }

  /** Slot metadata for a live device (by index), or empty role if unassigned. */
  OwSlot slotForDevice(size_t i) const {
    OwSlot empty;
    if (i >= count_) return empty;
    char hex[kOwAddrHexLen + 1];
    ow_addr_to_hex(devices_[i].addr, hex);
    int s = ow_slot_find(slots_, n_slots_, hex);
    if (s < 0) return empty;
    return slots_[s];
  }

  /** Assigned+fresh+healthy reading for a channel role, else invalid. */
  TempValue roleValue(OwRole role, unsigned long now) const {
    TempValue v;
    if (!enabled_ || (role != OW_ROLE_OUTDOOR && role != OW_ROLE_RETURN)) return v;
    const char* want = ow_addr_for_role(slots_, n_slots_, role);
    if (!want || !want[0]) return v;
    uint8_t raw[kOwAddrBytes];
    if (!ow_hex_to_addr(want, raw)) return v;
    for (size_t i = 0; i < count_; i++) {
      const OwDevice& d = devices_[i];
      if (memcmp(d.addr, raw, kOwAddrBytes) != 0) continue;
      bool fresh = d.valid && d.health == OW_HEALTH_OK &&
                   (now - d.ts_ms) <= kOwStaleMs;
      v.valid = fresh;
      v.celsius = d.celsius;
      return v;
    }
    return v;  // assigned but not on the bus right now
  }

  /**
   * Iterate custom sensors with a fresh OK reading.
   * Returns false when idx is past the last custom.
   * name_out must hold >= kOwNameMax+1.
   */
  bool customAt(size_t idx, char* name_out, float* celsius_out,
                unsigned long now) const {
    if (!enabled_ || !name_out || !celsius_out) return false;
    size_t seen = 0;
    for (size_t i = 0; i < n_slots_; i++) {
      if (slots_[i].role != OW_ROLE_CUSTOM || !slots_[i].addr[0]) continue;
      if (seen++ != idx) continue;
      // copy name
      size_t k = 0;
      for (; k < kOwNameMax && slots_[i].name[k]; k++) name_out[k] = slots_[i].name[k];
      name_out[k] = '\0';
      *celsius_out = NAN;
      uint8_t raw[kOwAddrBytes];
      if (!ow_hex_to_addr(slots_[i].addr, raw)) return true;
      for (size_t d = 0; d < count_; d++) {
        if (memcmp(devices_[d].addr, raw, kOwAddrBytes) != 0) continue;
        if (devices_[d].valid && devices_[d].health == OW_HEALTH_OK &&
            (now - devices_[d].ts_ms) <= kOwStaleMs) {
          *celsius_out = devices_[d].celsius;
        }
        return true;
      }
      return true;  // assigned but no reading
    }
    return false;
  }

 private:
  enum Phase : uint8_t { IDLE, CONVERTING };

  void request() {
    last_req_ms_ = millis();
    if (!count_) scan();
    if (!count_) return;
    bus_->requestTemperatures();
    phase_ = CONVERTING;
    req_ms_ = last_req_ms_;
  }

  void readAll() {
    for (size_t i = 0; i < count_; i++) {
      OwDevice& d = devices_[i];
      if (d.family != 0x28) {
        d.health = OW_HEALTH_UNSUPPORTED;
        d.valid = false;
        d.present = true;  // still on the bus
        continue;
      }
      // Presence check via DallasTemperature
      bool present = bus_->isConnected(d.addr);
      d.present = present;
      if (!present) {
        d.health = OW_HEALTH_DISCONNECTED;
        d.valid = false;
        continue;
      }
      float t = bus_->getTempC(d.addr);
      // DallasTemperature returns DEVICE_DISCONNECTED_C (-127) on bus/CRC fail
      OwHealth h = ow_classify(t, d.have_good, d.last_good_c);
      d.health = h;
      if (h == OW_HEALTH_OK) {
        d.celsius = t;
        d.valid = true;
        d.ts_ms = millis();
        d.last_good_c = t;
        d.have_good = true;
      } else {
        d.valid = false;
        // keep last good celsius for display until stale
      }
    }
  }

  int pin_ = HCS_ONEWIRE_PIN;
  bool enabled_ = false;
  std::unique_ptr<OneWire> wire_;
  std::unique_ptr<DallasTemperature> bus_;
  OwDevice devices_[kOwMaxSlots];
  size_t count_ = 0;
  OwSlot slots_[kOwMaxSlots];
  size_t n_slots_ = kOwMaxSlots;
  Phase phase_ = IDLE;
  unsigned long req_ms_ = 0, last_req_ms_ = -kOwPollMs;
};

}  // namespace hcs

#endif  // ARDUINO
