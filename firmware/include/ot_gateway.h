#pragma once
// OpenTherm gateway: thermostat bus (slave) + boiler bus (master) glue.
// ESP32 gateway builds only (HCS_GW_ENABLE); compiled out elsewhere.

#if defined(ESP32) && defined(HCS_GW_ENABLE)

#include <Arduino.h>
#include "ot_master.h"
#include "ot_slave.h"
#include "hcs_gateway.h"

namespace hcs {

class OtGateway {
 public:
  OtGateway(OtMaster& master, int tstat_in_pin, int tstat_out_pin)
      : m_(master), slave_(tstat_in_pin, tstat_out_pin) {}

  void begin() {
    rt_.reset();
    slave_.onResponseProvider(
        [this](uint32_t req) { return handleRequest(req); });
    slave_.begin();
  }

  void loop() { slave_.loop(); }

  bool thermostatOnline(unsigned long within_ms = 5000) const {
    return slave_.lastRequestMs() != 0 &&
           millis() - slave_.lastRequestMs() <= within_ms;
  }

  const GwCounters& counters() const { return rt_.counters(); }

  /** Force CH flow setpoint (°C) on TSet writes; NAN releases. */
  void setOverrideSetpointC(float c) { rt_.setOverrideSetpointC(c); }
  float overrideSetpointC() const { return rt_.overrideSetpointC(); }

 private:
  uint32_t handleRequest(uint32_t req);

  OtMaster& m_;
  OtSlave slave_;
  GatewayRouter rt_;
};

}  // namespace hcs

#endif  // ESP32 && HCS_GW_ENABLE
