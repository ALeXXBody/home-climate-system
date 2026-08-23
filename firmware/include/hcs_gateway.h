#pragma once
/**
 * Gateway router core for Home Climate System (portable, no Arduino deps).
 *
 * Decides what happens to every request frame arriving from the wall
 * thermostat while the device is in gateway mode:
 *
 *   Forward      -> pass (possibly modified) request to the boiler
 *   AnswerLocal  -> synthesise a response locally without touching the
 *                   boiler (used while the boiler link is down)
 *
 * The exact code runs on ESP32 and is unit-tested on the host via
 * `pio test -e native`. Compile-time gate for firmware use:
 * `#if defined(ESP32) && defined(HCS_GW_ENABLE)`.
 *
 * Data-value encoding helpers implement OpenTherm f8.8 (same layout the
 * ihormelnyk library uses) so no library types leak into this module.
 */

#include <math.h>
#include <stdint.h>
#include "hcs_gw_cfg.h"

namespace hcs {

// OpenTherm message types (3-bit field)
enum : uint8_t {
  kTypeReadData = 0,
  kTypeWriteData = 1,
  kTypeInvalidData = 2,
  kTypeReserved = 3,
  kTypeReadAck = 4,
  kTypeWriteAck = 5,
  kTypeDataInvalid = 6,
  kTypeUnknownDataId = 7,
};

// Message IDs used by the router
enum : uint8_t {
  kIdStatus = 0,
  kIdTSet = 1,
};

/** Encode °C to f8.8 (clamped to >= 0). */
inline uint16_t f88_encode(float c) {
  if (isnan(c)) return 0;
  if (c < 0.0f) c = 0.0f;
  float v = c * 256.0f;
  if (v > 65535.0f) v = 65535.0f;
  return (uint16_t)(v + 0.5f);
}

/** Decode f8.8 to °C. */
inline float f88_decode(uint16_t d) { return (float)d / 256.0f; }

enum class GwPolicy : uint8_t { Forward, AnswerLocal };

struct GwCounters {
  uint32_t requests = 0;
  uint32_t forwarded = 0;
  uint32_t answered_local = 0;
  uint32_t modified = 0;
  uint32_t errors = 0;
};

class GatewayRouter {
 public:
  void reset();

  /**
   * Route one thermostat request.
   *  type/id/data: decoded request fields (data = 16-bit payload)
   *  out_forward_data: payload to forward when returning Forward
   *  out_local_data:   payload for the local response when returning
   *                    AnswerLocal (valid only if local_answer_known())
   */
  GwPolicy route(uint8_t type, uint8_t id, uint16_t data,
                 uint16_t* out_forward_data);

  /** True when the most recent AnswerLocal decision had cached data. */
  bool local_answer_known() const { return local_known_; }

  /** Record a successful boiler response (keeps the local-answer cache warm). */
  void noteBoilerResponse(uint8_t resp_type, uint8_t id, uint16_t data);

  // --- overrides -----------------------------------------------------------
  /** Force CH flow setpoint (°C) on TSet writes; NAN disables the override. */
  void setOverrideSetpointC(float c) { ov_setpoint_c_ = c; }
  float overrideSetpointC() const { return ov_setpoint_c_; }

  // --- boiler link state ---------------------------------------------------
  /** While false, all requests are answered locally (never forwarded). */
  void setBoilerLinkUp(bool up) { boiler_up_ = up; }
  bool boilerLinkUp() const { return boiler_up_; }

  const GwCounters& counters() const { return cnt_; }

 private:
  static constexpr uint8_t kCacheN = 8;
  struct CacheEntry {
    bool used = false;
    uint8_t id = 0;
    uint16_t data = 0;
  };

  int cacheFind(uint8_t id) const;
  void cachePut(uint8_t id, uint16_t data);

  GwCounters cnt_;
  float ov_setpoint_c_ = (float)NAN;
  bool boiler_up_ = true;
  bool local_known_ = false;
  CacheEntry cache_[kCacheN];
};

// ---------------------------------------------------------------------------
// Inline implementation (header-only so host-native tests need no src/ build)
// ---------------------------------------------------------------------------

inline void GatewayRouter::reset() {
  cnt_ = GwCounters();
  ov_setpoint_c_ = (float)NAN;
  boiler_up_ = true;
  local_known_ = false;
  for (auto& e : cache_) e = CacheEntry();
}

inline int GatewayRouter::cacheFind(uint8_t id) const {
  for (int i = 0; i < kCacheN; i++) {
    if (cache_[i].used && cache_[i].id == id) return i;
  }
  return -1;
}

inline void GatewayRouter::cachePut(uint8_t id, uint16_t data) {
  int i = cacheFind(id);
  if (i < 0) {
    // first free slot; if none, replace slot 0 (simple ring of one)
    for (int j = 0; j < kCacheN && i < 0; j++) {
      if (!cache_[j].used) i = j;
    }
    if (i < 0) i = 0;
  }
  cache_[i].used = true;
  cache_[i].id = id;
  cache_[i].data = data;
}

inline GwPolicy GatewayRouter::route(uint8_t type, uint8_t id, uint16_t data,
                                     uint16_t* out_forward_data) {
  cnt_.requests++;
  local_known_ = false;

  // Boiler link down -> never forward; answer from cache if possible
  if (!boiler_up_) {
    int i = cacheFind(id);
    if (i >= 0) {
      local_known_ = true;
      if (out_forward_data) *out_forward_data = cache_[i].data;
      cnt_.answered_local++;
      return GwPolicy::AnswerLocal;
    }
    cnt_.answered_local++;
    return GwPolicy::AnswerLocal;  // caller sends UNKNOWN-DATA-ID / DATA-INVALID
  }

  uint16_t out = data;

  // Setpoint override: rewrite WRITE_DATA(TSet) payloads
  if (id == kIdTSet && type == kTypeWriteData && !isnan(ov_setpoint_c_)) {
    uint16_t forced = f88_encode(ov_setpoint_c_);
    if (forced != out) cnt_.modified++;
    out = forced;
  }

  if (out_forward_data) *out_forward_data = out;
  cnt_.forwarded++;
  return GwPolicy::Forward;
}

inline void GatewayRouter::noteBoilerResponse(uint8_t resp_type, uint8_t id,
                                              uint16_t data) {
  (void)resp_type;
  if (resp_type == kTypeDataInvalid || resp_type == kTypeUnknownDataId) return;
  cachePut(id, data);
}

/**
 * Auto-detect decision (portable, unit-tested).
 *
 * Called periodically while probing the thermostat-side bus at boot.
 * Returns 0 while undecided, HCS_GW_GATEWAY once the window elapsed with
 * >= kGwAutoMinFrames valid requests seen, else HCS_GW_MASTER_ONLY
 * (values match enum HcsGwCfg in settings_store.h).
 */
constexpr unsigned long kGwAutoWindowMs = 15000;
constexpr uint32_t kGwAutoMinFrames = 2;

inline int gw_autodetect_decide(uint32_t valid_requests,
                                unsigned long elapsed_ms,
                                unsigned long window_ms = kGwAutoWindowMs) {
  if (elapsed_ms < window_ms) return 0;  // keep listening
  return valid_requests >= kGwAutoMinFrames ? (int)HCS_GW_GATEWAY
                                            : (int)HCS_GW_MASTER_ONLY;
}

}  // namespace hcs
