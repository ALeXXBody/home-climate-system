#pragma once
/**
 * Pure decision helpers for the Wi-Fi self-heal escalation (v1.5.6).
 * Factored out of NetServices::bulletproofWifiTick so the thresholds are
 * verified on-host (native test suite) as well as on-device.
 *
 * Background: an ESP32 Wi-Fi driver can wedge into a state where neither
 * reconnect() nor a full dissociate+begin recovers, and the board then sits
 * OT-alive but unreachable for hours (11 h observed after the 1.5.5 OTA).
 * The only cure for a wedged stack is a clean restart — hence the last
 * escalation level below.
 */
#include <stdint.h>

namespace hcs {

enum class WifiHealAction : uint8_t {
  NONE = 0,     // connected, or still within soft/forced retry path
  RESTART = 1,  // driver wedged: schedule a clean restart
};

inline WifiHealAction wifi_heal_decide(bool connected,
                                       uint32_t down_ms,
                                       unsigned int force_count,
                                       bool reboot_pending) {
  constexpr uint32_t kRestartAfterMs = 600000;  // 10 minutes solid outage
  constexpr unsigned int kMinForced = 2;        // full re-association tried first
  if (connected) return WifiHealAction::NONE;
  if (reboot_pending) return WifiHealAction::NONE;  // a reboot is already queued
  if (down_ms >= kRestartAfterMs && force_count >= kMinForced)
    return WifiHealAction::RESTART;
  return WifiHealAction::NONE;
}

}  // namespace hcs
