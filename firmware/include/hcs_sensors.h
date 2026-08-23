#pragma once
// DS18B20 1-Wire manager for Home Climate System.
// Arduino builds only; portable rules live in hcs_sensor_logic.h.

#ifdef ARDUINO

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <memory>
#include "hcs_sensor_logic.h"

#ifndef HCS_ONEWIRE_PIN
#define HCS_ONEWIRE_PIN (-1)  // per-env override in platformio.ini; -1 = absent
#endif

namespace hcs {

constexpr size_t kOwMaxDevices = 8;
constexpr unsigned long kOwPollMs = 15000;      // conversion request cadence
constexpr unsigned long kOwConvMs = 800;        // 12-bit conversion time
constexpr unsigned long kOwStaleMs = 90000;     // reading older than this = stale

struct OwDevice {
  uint8_t addr[kOwAddrBytes] = {0};
  bool valid = false;       // temp read OK at some point
  float celsius = NAN;
  unsigned long ts_ms = 0;  // millis() of last successful read
};

class HcsSensors {
 public:
  /** Wire roles from settings. Empty string clears the assignment. */
  void configure(bool enabled, const char* outdoor_hex, const char* return_hex) {
    enabled_ = enabled;
    outdoor_assigned_ = ow_hex_to_addr(outdoor_hex, outdoor_addr_);
    return_assigned_ = ow_hex_to_addr(return_hex, return_addr_);
    // same probe cannot hold both roles
    if (outdoor_assigned_ && return_assigned_ &&
        memcmp(outdoor_addr_, return_addr_, kOwAddrBytes) == 0) {
      return_assigned_ = false;
    }
  }

  void begin() {
    if (HCS_ONEWIRE_PIN < 0) return;
    pin_ = HCS_ONEWIRE_PIN;
    wire_.reset(new OneWire(pin_));
    bus_.reset(new DallasTemperature(wire_.get()));
    bus_->setWaitForConversion(false);  // non-blocking pattern
    scan();
    request();
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
        if (now - req_ms_ >= kOwConvMs) read();
        break;
    }
  }

  /** Re-enumerate devices on the bus (also runs at begin()). */
  void scan() {
    if (!bus_) return;
    count_ = 0;
    wire_->reset_search();
    while (count_ < kOwMaxDevices && wire_->search(devices_[count_].addr)) {
      count_++;
    }
  }

  bool enabled() const { return enabled_; }
  int pin() const { return pin_; }
  size_t count() const { return count_; }
  const OwDevice& device(size_t i) const { return devices_[i]; }

  bool outdoorAssigned() const { return outdoor_assigned_; }
  bool returnAssigned() const { return return_assigned_; }

  /** Assigned+fresh reading for a role, else invalid. */
  TempValue roleValue(OwRole role, unsigned long now) const {
    TempValue v;
    const uint8_t* want =
        (role == OW_ROLE_OUTDOOR) ? outdoor_addr_ : return_addr_;
    bool assigned = (role == OW_ROLE_OUTDOOR) ? outdoor_assigned_
                                              : return_assigned_;
    if (!assigned || !enabled_) return v;
    for (size_t i = 0; i < count_; i++) {
      const OwDevice& d = devices_[i];
      if (memcmp(d.addr, want, kOwAddrBytes) != 0) continue;
      v.valid = d.valid && (now - d.ts_ms) <= kOwStaleMs;
      v.celsius = d.celsius;
      return v;
    }
    return v;  // assigned but not on the bus right now
  }

 private:
  enum Phase : uint8_t { IDLE, CONVERTING };

  void request() {
    last_req_ms_ = millis();
    if (!count_) { scan(); }
    if (!count_) return;
    bus_->requestTemperatures();
    phase_ = CONVERTING;
    req_ms_ = last_req_ms_;
  }

  void read() {
    phase_ = IDLE;
    for (size_t i = 0; i < count_; i++) {
      float t = bus_->getTempC(devices_[i].addr);
      if (t == DEVICE_DISCONNECTED_C || t <= -100) continue;
      devices_[i].celsius = t;
      devices_[i].valid = true;
      devices_[i].ts_ms = millis();
    }
  }

  int pin_ = HCS_ONEWIRE_PIN;
  bool enabled_ = false;
  std::unique_ptr<OneWire> wire_;
  std::unique_ptr<DallasTemperature> bus_;
  OwDevice devices_[kOwMaxDevices];
  size_t count_ = 0;
  Phase phase_ = IDLE;
  unsigned long req_ms_ = 0, last_req_ms_ = -kOwPollMs;
  uint8_t outdoor_addr_[kOwAddrBytes] = {0};
  uint8_t return_addr_[kOwAddrBytes] = {0};
  bool outdoor_assigned_ = false, return_assigned_ = false;
};

}  // namespace hcs

#endif  // ARDUINO
