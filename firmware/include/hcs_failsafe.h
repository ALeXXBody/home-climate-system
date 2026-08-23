#pragma once
/**
 * Connection-loss failsafe policy for Home Climate System (portable,
 * host-testable).
 *
 * Intent: if the head-end (WiFi/MQTT) disappears in freezing weather, the
 * box must keep the house warm using owner-chosen values instead of
 * silently holding or dropping heat.
 *
 * State machine (evaluated every loop):
 *   CONNECTED   -> link up
 *   HOLD        -> link lost, inside grace window: run last state
 *   FAILSAFE    -> link lost beyond grace: force CH at fs flow setpoint,
 *                  weather compensation bypassed for predictability
 */

#include <stdint.h>

namespace hcs {

// Owner-editable defaults (overridable at runtime, persisted)
constexpr bool kFsEnableDefault = true;
constexpr float kFsFlowDefaultC = 40.0f;
constexpr uint8_t kFsGraceDefaultMin = 10;

enum class FsState : uint8_t {
  CONNECTED = 0,
  HOLD = 1,
  FAILSAFE = 2,
};

/**
 * Evaluate the failsafe state.
 * @param enabled       owner switch (false -> always CONNECTED semantics)
 * @param link_up       WiFi associated AND (no mqtt configured OR mqtt connected)
 * @param lost_ms       how long the link has been down (0 while up)
 * @param grace_ms      owner grace period before failsafe engages
 */
inline FsState fs_evaluate(bool enabled, bool link_up, unsigned long lost_ms,
                           unsigned long grace_ms) {
  if (!enabled || link_up) return FsState::CONNECTED;
  return (lost_ms >= grace_ms) ? FsState::FAILSAFE : FsState::HOLD;
}

/** Effective CH demand in the given state (last_cmd = current relay state). */
inline bool fs_ch_demand(FsState st, bool last_cmd) {
  if (st == FsState::FAILSAFE) return true;  // freeze protection
  return last_cmd;
}

}  // namespace hcs
