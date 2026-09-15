#pragma once
// Slave-side OpenTherm endpoint facing the wall thermostat.
// ESP32 gateway builds only (HCS_GW_ENABLE).
//
// INTERRUPT SAFETY (must not regress):
// The ISR is our own IRAM_ATTR decoder using direct GPIO register reads —
// NO library calls (the library's handleInterrupt() reaches digitalRead(),
// which is flash-resident and panics when an edge fires during a flash/NVS
// operation with the cache disabled). The ISR only latches the decoded
// request frame and raises a volatile flag; the response (provider lookup
// + bit-bang transmission) runs in loop() task context.

#if defined(ESP32) && defined(HCS_GW_ENABLE)

#include <Arduino.h>
#include <OpenTherm.h>
#include "soc/gpio_struct.h"
#include "soc/gpio_reg.h"
#include "soc/soc.h"

namespace hcs {

class OtSlave {
 public:
  using ResponseProvider = std::function<uint32_t(uint32_t request)>;

  OtSlave(int in_pin, int out_pin) : in_pin_(in_pin), out_pin_(out_pin) {}

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
  /** Frames that decoded as valid OpenTherm requests. */
  uint32_t validRequests() const { return valid_; }

  static OtSlave* instance() { return instance_; }

  /** The all-IRAM decode path (attached via attachInterrupt). */
  static void IRAM_ATTR isr();

 private:
  enum IsrState : uint8_t { IS_READY = 0, COLLECTING };

  static inline bool IRAM_ATTR fastRead_(uint8_t pin) {
    // Direct register read: works across ALL ESP32 variants regardless of
    // how the gpio_dev_s struct nests its members (plain struct on
    // ESP32/S2, union-based on C3/S3 — same memory layout for RAW input).
    return (REG_READ(GPIO_IN_REG) >> pin) & 0x1u;
  }

  void service();
  void txAnswer_(uint32_t resp);
  void txBit_(bool high);

  static OtSlave* instance_;
  int in_pin_, out_pin_;
  ResponseProvider provider_;

  // ISR-owned volatile state
  volatile IsrState isr_state_ = IS_READY;
  volatile unsigned long isr_ts_ = 0;
  volatile uint32_t isr_bits_ = 0;
  volatile uint8_t isr_idx_ = 0;

  volatile bool pending_ = false;
  volatile uint32_t req_ = 0;      // latched master request frame
  unsigned long last_req_ms_ = 0;
  uint32_t seen_ = 0;
  uint32_t valid_ = 0;
  uint32_t answered_ = 0;
};

}  // namespace hcs

#endif  // ESP32 && HCS_GW_ENABLE
