#pragma once
// Slave-side OpenTherm endpoint facing the wall thermostat.
// ESP32 gateway builds only (HCS_GW_ENABLE); compiled out elsewhere.

#if defined(ESP32) && defined(HCS_GW_ENABLE)

#include <Arduino.h>
#include <OpenTherm.h>

namespace hcs {

class OtSlave {
 public:
  using ResponseProvider = std::function<uint32_t(uint32_t request)>;

  OtSlave(int in_pin, int out_pin) : ot_(in_pin, out_pin, true) {}

  void begin();
  /** Call frequently from loop(); answers pending requests via provider. */
  void loop();

  /**
   * Must return a complete slave->master frame for the given master->slave
   * request, or 0 to answer UNKNOWN-DATA-ID. Invoked in loop() context,
   * never in ISR.
   */
  void onResponseProvider(ResponseProvider fn) { provider_ = fn; }

  bool pending() const { return pending_; }
  unsigned long lastRequestMs() const { return last_req_ms_; }
  uint32_t framesSeen() const { return seen_; }
  /** Frames that decoded as valid OpenTherm requests (SUCCESS). */
  uint32_t validRequests() const { return valid_; }

 private:
  static OtSlave* instance_;
  void onFrame(unsigned long frame, OpenThermResponseStatus st);

  OpenTherm ot_;
  ResponseProvider provider_;
  volatile bool pending_ = false;
  uint32_t req_ = 0;
  unsigned long last_req_ms_ = 0;
  uint32_t seen_ = 0;
  uint32_t valid_ = 0;
};

}  // namespace hcs

#endif  // ESP32 && HCS_GW_ENABLE
