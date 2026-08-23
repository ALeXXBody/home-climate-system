#pragma once
// Gateway role selection shared by firmware and host-side unit tests.
// Kept free of Arduino types so native tests can include it.

namespace hcs {

/** Gateway role selection. AUTO probes for a thermostat bus at boot. */
enum HcsGwCfg : uint8_t {
  HCS_GW_AUTO = 0,         ///< listen on OT2 at boot; promote if thermostat seen
  HCS_GW_MASTER_ONLY = 1,  ///< forced master (boiler side only)
  HCS_GW_GATEWAY = 2       ///< forced master+slave gateway
};

/** Human-readable name for a HcsGwCfg value ("auto"|"master_only"|"gateway"). */
inline const char* hcs_gw_cfg_name(uint8_t cfg) {
  switch (cfg) {
    case HCS_GW_GATEWAY: return "gateway";
    case HCS_GW_MASTER_ONLY: return "master_only";
    default: return "auto";
  }
}

}  // namespace hcs
