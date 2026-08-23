#include <string.h>
#include <math.h>
#include <initializer_list>
#include <unity.h>
#include "hcs_commands.h"
#include "hcs_gateway.h"
#include "hcs_weather_comp.h"
#include "hcs_sensor_logic.h"
#include "hcs_boiler_text.h"
#include "hcs_ot_caps.h"
#include "hcs_failsafe.h"

void setUp(void) {}
void tearDown(void) {}

static HcsCommandResult parse(const char* topic, const char* payload) {
  return hcs_parse_command(topic, payload);
}

// ==================== MQTT command parser ====================
// ---------- ch_enable / chenable ----------
void test_ch_enable_on_variants(void) {
  const char* payloads[] = {"on", "ON", "On", "1", "true", "TRUE"};
  for (auto p : payloads) {
    HcsCommandResult r = parse("hcs/node/set/ch_enable", p);
    TEST_ASSERT_EQUAL_MESSAGE(HCS_CMD_CH_ENABLE, r.cmd, p);
    TEST_ASSERT_TRUE_MESSAGE(r.bool_value, p);
  }
}

void test_ch_enable_off_variants(void) {
  const char* payloads[] = {"off", "OFF", "0", "false", ""};
  for (auto p : payloads) {
    HcsCommandResult r = parse("hcs/node/set/ch_enable", p);
    TEST_ASSERT_EQUAL_MESSAGE(HCS_CMD_CH_ENABLE, r.cmd, p);
    TEST_ASSERT_FALSE_MESSAGE(r.bool_value, p);
  }
}

void test_otgw_chenable_alias(void) {
  HcsCommandResult r = parse("OTGW/set/hcs-device/chenable", "on");
  TEST_ASSERT_EQUAL(HCS_CMD_CH_ENABLE, r.cmd);
  TEST_ASSERT_TRUE(r.bool_value);
}

// ---------- dhw_enable ----------
void test_dhw_enable_roundtrip(void) {
  HcsCommandResult on = parse("hcs/n/set/dhw_enable", "1");
  TEST_ASSERT_EQUAL(HCS_CMD_DHW_ENABLE, on.cmd);
  TEST_ASSERT_TRUE(on.bool_value);

  HcsCommandResult off = parse("hcs/n/set/dhw_enable", "false");
  TEST_ASSERT_EQUAL(HCS_CMD_DHW_ENABLE, off.cmd);
  TEST_ASSERT_FALSE(off.bool_value);
}

// ---------- flow_setpoint / ctrlsetpt ----------
void test_flow_setpoint_floats(void) {
  HcsCommandResult a = parse("hcs/n/set/flow_setpoint", "42.5");
  TEST_ASSERT_EQUAL(HCS_CMD_FLOW_SETPOINT, a.cmd);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 42.5, a.float_value);

  HcsCommandResult b = parse("OTGW/set/node/ctrlsetpt", "-5");
  TEST_ASSERT_EQUAL(HCS_CMD_FLOW_SETPOINT, b.cmd);
  TEST_ASSERT_FLOAT_WITHIN(0.01, -5.0, b.float_value);
}

void test_flow_setpoint_garbage_is_zero(void) {
  HcsCommandResult r = parse("hcs/n/set/flow_setpoint", "abc");
  TEST_ASSERT_EQUAL(HCS_CMD_FLOW_SETPOINT, r.cmd);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 0.0, r.float_value);
}

// ---------- max_modulation clamps 0..100 ----------
void test_max_modulation_clamped(void) {
  HcsCommandResult hi = parse("hcs/n/set/max_modulation", "150");
  TEST_ASSERT_EQUAL(HCS_CMD_MAX_MODULATION, hi.cmd);
  TEST_ASSERT_EQUAL(100, hi.int_value);

  HcsCommandResult lo = parse("hcs/n/set/max_modulation", "-3");
  TEST_ASSERT_EQUAL(0, lo.int_value);

  HcsCommandResult ok = parse("OTGW/set/node/maxmodulation", "77");
  TEST_ASSERT_EQUAL(77, ok.int_value);
}

// ---------- reboot / ota_url ----------
void test_reboot_and_ota_url(void) {
  HcsCommandResult rb = parse("hcs/n/set/reboot", "");
  TEST_ASSERT_EQUAL(HCS_CMD_REBOOT, rb.cmd);

  HcsCommandResult ota = parse("hcs/n/set/ota_url", "http://x/fw.bin");
  TEST_ASSERT_EQUAL(HCS_CMD_OTA_URL, ota.cmd);
}

// ---------- unknown topics & safety ----------
void test_unknown_topic_ignored(void) {
  HcsCommandResult r = parse("hcs/n/set/not_a_command", "1");
  TEST_ASSERT_EQUAL(HCS_CMD_NONE, r.cmd);
}

void test_prefix_must_not_leak_into_other_commands(void) {
  // A topic that merely CONTAINS "/ch_enable" but doesn't end with it
  HcsCommandResult r = parse("hcs/n/set/ch_enable/extra", "on");
  TEST_ASSERT_EQUAL(HCS_CMD_NONE, r.cmd);
}

void test_null_inputs_safe(void) {
  HcsCommandResult r = hcs_parse_command(nullptr, nullptr);
  TEST_ASSERT_EQUAL(HCS_CMD_NONE, r.cmd);
}

// ==================== weather compensation ====================
static HcsWeatherComp wcDef() {
  HcsWeatherComp wc;
  wc.enable = true;
  return wc;  // ref 18, design -10, max 65, min 25
}

// ---------- target computation ----------
void test_wc_disabled_is_nan(void) {
  HcsWeatherComp wc = wcDef();
  wc.enable = false;
  TEST_ASSERT(isnan(hcs_weather_comp_target(wc, 5.0f)));
}

void test_wc_nan_outdoor_is_nan(void) {
  HcsWeatherComp wc = wcDef();
  TEST_ASSERT(isnan(hcs_weather_comp_target(wc, NAN)));
}

void test_wc_at_reference_returns_flow_min(void) {
  HcsWeatherComp wc = wcDef();
  TEST_ASSERT_FLOAT_WITHIN(0.01, 25.0, hcs_weather_comp_target(wc, 18.0f));
}

void test_wc_above_reference_clamps_to_flow_min(void) {
  HcsWeatherComp wc = wcDef();
  TEST_ASSERT_FLOAT_WITHIN(0.01, 25.0, hcs_weather_comp_target(wc, 21.5f));
}

void test_wc_at_design_returns_flow_max(void) {
  HcsWeatherComp wc = wcDef();
  TEST_ASSERT_FLOAT_WITHIN(0.01, 65.0, hcs_weather_comp_target(wc, -10.0f));
}

void test_wc_below_design_clamps_to_flow_max(void) {
  HcsWeatherComp wc = wcDef();
  TEST_ASSERT_FLOAT_WITHIN(0.01, 65.0, hcs_weather_comp_target(wc, -22.0f));
}

void test_wc_midpoint_interpolates(void) {
  // halfway between design(-10) and ref(18) -> midpoint of 25..65 = 45
  HcsWeatherComp wc = wcDef();
  TEST_ASSERT_FLOAT_WITHIN(0.05, 45.0, hcs_weather_comp_target(wc, 4.0f));
}

void test_wc_quarter_point_interpolates(void) {
  // t_out = -4 -> frac=(18-(-4))/28=0.7857 -> 25+0.7857*40=56.43
  HcsWeatherComp wc = wcDef();
  TEST_ASSERT_FLOAT_WITHIN(0.05, 56.43, hcs_weather_comp_target(wc, -4.0f));
}

// ---------- config parsing ----------
void test_wc_cfg_parse_valid(void) {
  HcsWeatherComp wc;
  TEST_ASSERT_TRUE(hcs_weather_comp_parse_cfg("20,-15,70,30", wc));
  TEST_ASSERT_FLOAT_WITHIN(0.01, 20.0, wc.t_out_ref);
  TEST_ASSERT_FLOAT_WITHIN(0.01, -15.0, wc.t_out_design);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 70.0, wc.flow_max);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 30.0, wc.flow_min);
}

void test_wc_cfg_parse_rejects_garbage(void) {
  HcsWeatherComp wc;
  TEST_ASSERT_FALSE(hcs_weather_comp_parse_cfg("abc", wc));
  TEST_ASSERT_FALSE(hcs_weather_comp_parse_cfg("18,-10,65", wc));   // short
  TEST_ASSERT_FALSE(hcs_weather_comp_parse_cfg(nullptr, wc));
  TEST_ASSERT_FALSE(hcs_weather_comp_parse_cfg("18,-10,,25", wc));  // hole
}

void test_wc_cfg_parse_rejects_contradictory(void) {
  HcsWeatherComp wc;
  TEST_ASSERT_FALSE(hcs_weather_comp_parse_cfg("18,-10,20,50", wc));  // max<min
  TEST_ASSERT_FALSE(hcs_weather_comp_parse_cfg("18,18,65,25", wc));   // design==ref
  TEST_ASSERT_FALSE(hcs_weather_comp_parse_cfg("-5,5,65,25", wc));    // design>ref
  TEST_ASSERT_FALSE(hcs_weather_comp_parse_cfg("18,-10,95,25", wc));  // unsafe max
  TEST_ASSERT_FALSE(hcs_weather_comp_parse_cfg("18,-10,65,5", wc));   // unsafe min
}

void test_wc_cfg_design_below_reference_is_valid(void) {
  HcsWeatherComp wc;
  TEST_ASSERT_TRUE(hcs_weather_comp_parse_cfg("10,-10,65,25", wc));  // cold climate
}

void test_wc_cfg_failure_leaves_struct_untouched(void) {
  HcsWeatherComp wc;
  HcsWeatherComp before = wc;
  TEST_ASSERT_FALSE(hcs_weather_comp_parse_cfg("bad,input", wc));
  TEST_ASSERT_FLOAT_WITHIN(0.01, before.t_out_ref, wc.t_out_ref);
  TEST_ASSERT_FLOAT_WITHIN(0.01, before.flow_max, wc.flow_max);
}

// ==================== gateway router ====================
static const uint16_t kSet45 = 45 * 256;  // f8.8

void test_gw_forward_passthrough_by_default(void) {
  hcs::GatewayRouter r;
  uint16_t out = 0;
  TEST_ASSERT_EQUAL(hcs::GwPolicy::Forward,
                    r.route(hcs::kTypeReadData, hcs::kIdStatus, 0x0100, &out));
  TEST_ASSERT_EQUAL_HEX16(0x0100, out);
  TEST_ASSERT_EQUAL(1, r.counters().requests);
  TEST_ASSERT_EQUAL(1, r.counters().forwarded);
  TEST_ASSERT_EQUAL(0, r.counters().modified);
}

void test_gw_setpoint_override_rewrites_tset(void) {
  hcs::GatewayRouter r;
  r.setOverrideSetpointC(50.0f);
  uint16_t out = 0;
  TEST_ASSERT_EQUAL(hcs::GwPolicy::Forward,
                    r.route(hcs::kTypeWriteData, hcs::kIdTSet, kSet45, &out));
  TEST_ASSERT_EQUAL_HEX16(50 * 256, out);
  TEST_ASSERT_EQUAL(1, r.counters().modified);
  // same payload again -> no double count
  r.route(hcs::kTypeWriteData, hcs::kIdTSet, 50 * 256, &out);
  TEST_ASSERT_EQUAL(1, r.counters().modified);
}

void test_gw_override_ignores_other_ids_and_reads(void) {
  hcs::GatewayRouter r;
  r.setOverrideSetpointC(60.0f);
  uint16_t out = 0xFFFF;
  r.route(hcs::kTypeWriteData, hcs::kIdStatus, kSet45, &out);   // wrong id
  TEST_ASSERT_EQUAL_HEX16(kSet45, out);
  r.route(hcs::kTypeReadData, hcs::kIdTSet, kSet45, &out);      // read, not write
  TEST_ASSERT_EQUAL_HEX16(kSet45, out);
  TEST_ASSERT_EQUAL(0, r.counters().modified);
}

void test_gw_override_disable_with_nan(void) {
  hcs::GatewayRouter r;
  r.setOverrideSetpointC(55.0f);
  r.setOverrideSetpointC((float)NAN);
  uint16_t out = 0;
  r.route(hcs::kTypeWriteData, hcs::kIdTSet, kSet45, &out);
  TEST_ASSERT_EQUAL_HEX16(kSet45, out);
}

void test_gw_link_down_answers_local_from_cache(void) {
  hcs::GatewayRouter r;
  r.noteBoilerResponse(hcs::kTypeReadAck, hcs::kIdTSet, kSet45);
  r.setBoilerLinkUp(false);
  uint16_t out = 0;
  TEST_ASSERT_EQUAL(hcs::GwPolicy::AnswerLocal,
                    r.route(hcs::kTypeReadData, hcs::kIdTSet, 0, &out));
  TEST_ASSERT_TRUE(r.local_answer_known());
  TEST_ASSERT_EQUAL_HEX16(kSet45, out);
  TEST_ASSERT_EQUAL(1, r.counters().answered_local);
  TEST_ASSERT_EQUAL(0, r.counters().forwarded);
}

void test_gw_link_down_unknown_id_is_unsupported_local(void) {
  hcs::GatewayRouter r;
  r.setBoilerLinkUp(false);
  uint16_t out = 1234;
  TEST_ASSERT_EQUAL(hcs::GwPolicy::AnswerLocal,
                    r.route(hcs::kTypeWriteData, 42, 7, &out));
  TEST_ASSERT_FALSE(r.local_answer_known());
  TEST_ASSERT_EQUAL(0, r.counters().forwarded);
}

void test_gw_cache_skips_invalid_responses(void) {
  hcs::GatewayRouter r;
  r.noteBoilerResponse(hcs::kTypeDataInvalid, hcs::kIdTSet, kSet45);
  r.noteBoilerResponse(hcs::kTypeUnknownDataId, hcs::kIdStatus, 0x11);
  r.setBoilerLinkUp(false);
  uint16_t out = 0;
  r.route(hcs::kTypeReadData, hcs::kIdTSet, 0, &out);
  TEST_ASSERT_FALSE(r.local_answer_known());
}

void test_gw_link_up_always_forwards_even_if_cached(void) {
  hcs::GatewayRouter r;
  r.noteBoilerResponse(hcs::kTypeReadAck, hcs::kIdTSet, kSet45);
  uint16_t out = 0;
  TEST_ASSERT_EQUAL(hcs::GwPolicy::Forward,
                    r.route(hcs::kTypeWriteData, hcs::kIdTSet, 40 * 256, &out));
  TEST_ASSERT_EQUAL_HEX16(40 * 256, out);
}

void test_gw_f88_roundtrip(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 45.0f, hcs::f88_decode(hcs::f88_encode(45.0f)));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.5f + 0.5f, hcs::f88_decode(hcs::f88_encode(-3.0f)));  // clamped to 0
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, hcs::f88_encode(999.0f));                                 // clamp high
}

// ---------- gateway commands ----------
void test_gw_set_mode_parse(void) {
  HcsCommandResult gw = parse("hcs/n/set/gw/set_mode", "gateway");
  TEST_ASSERT_EQUAL(HCS_CMD_GW_MODE, gw.cmd);
  TEST_ASSERT_EQUAL_INT(hcs::HCS_GW_GATEWAY, gw.int_value);

  HcsCommandResult mo = parse("hcs/n/set/gw/set_mode", "master_only");
  TEST_ASSERT_EQUAL(HCS_CMD_GW_MODE, mo.cmd);
  TEST_ASSERT_EQUAL_INT(hcs::HCS_GW_MASTER_ONLY, mo.int_value);

  HcsCommandResult au = parse("hcs/n/set/gw/set_mode", "auto");
  TEST_ASSERT_EQUAL(HCS_CMD_GW_MODE, au.cmd);
  TEST_ASSERT_EQUAL_INT(hcs::HCS_GW_AUTO, au.int_value);

  HcsCommandResult legacy = parse("hcs/n/set/gw/set_mode", "1");
  TEST_ASSERT_EQUAL(HCS_CMD_GW_MODE, legacy.cmd);
  TEST_ASSERT_EQUAL_INT(hcs::HCS_GW_GATEWAY, legacy.int_value);

  HcsCommandResult junk = parse("hcs/n/set/gw/set_mode", "banana");
  TEST_ASSERT_EQUAL(HCS_CMD_GW_MODE, junk.cmd);
  // anything unknown falls back to the legacy bool mapping -> master_only
  TEST_ASSERT_EQUAL_INT(hcs::HCS_GW_MASTER_ONLY, junk.int_value);
}

// ---------- gateway role auto-detect ----------
void test_gw_autodetect_undecided_before_window(void) {
  // even many valid frames mean nothing until the window elapses
  TEST_ASSERT_EQUAL_INT(0, hcs::gw_autodetect_decide(50, 0));
  TEST_ASSERT_EQUAL_INT(0, hcs::gw_autodetect_decide(50, 14999));
}

void test_gw_autodetect_master_when_silent(void) {
  TEST_ASSERT_EQUAL_INT((int)hcs::HCS_GW_MASTER_ONLY,
                        hcs::gw_autodetect_decide(0, hcs::kGwAutoWindowMs));
  TEST_ASSERT_EQUAL_INT((int)hcs::HCS_GW_MASTER_ONLY,
                        hcs::gw_autodetect_decide(1, hcs::kGwAutoWindowMs + 1));
}

void test_gw_autodetect_gateway_with_two_frames(void) {
  TEST_ASSERT_EQUAL_INT(
      (int)hcs::HCS_GW_GATEWAY, hcs::gw_autodetect_decide(2, hcs::kGwAutoWindowMs));
  TEST_ASSERT_EQUAL_INT(
      (int)hcs::HCS_GW_GATEWAY,
      hcs::gw_autodetect_decide(100, hcs::kGwAutoWindowMs * 3));
}

void test_gw_autodetect_custom_window(void) {
  TEST_ASSERT_EQUAL_INT(0, hcs::gw_autodetect_decide(5, 4999, 5000));
  TEST_ASSERT_EQUAL_INT((int)hcs::HCS_GW_GATEWAY,
                        hcs::gw_autodetect_decide(5, 5000, 5000));
}


void test_gw_override_parse(void) {
  HcsCommandResult set = parse("hcs/n/set/gw/override_setpoint", "48.5");
  TEST_ASSERT_EQUAL(HCS_CMD_GW_OVERRIDE_SETPOINT, set.cmd);
  TEST_ASSERT_TRUE(set.bool_value);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 48.5, set.float_value);

  for (const char* rel : {"off", "auto", "release"}) {
    HcsCommandResult r = parse("hcs/n/set/gw/override_setpoint", rel);
    TEST_ASSERT_EQUAL_MESSAGE(HCS_CMD_GW_OVERRIDE_SETPOINT, r.cmd, rel);
    TEST_ASSERT_FALSE_MESSAGE(r.bool_value, rel);
  }
}

void test_gw_topics_do_not_shadow_flow_setpoint(void) {
  HcsCommandResult r = parse("hcs/n/set/flow_setpoint", "42");
  TEST_ASSERT_EQUAL(HCS_CMD_FLOW_SETPOINT, r.cmd);
}

// ---------- 1-Wire sensor logic ----------
void test_ow_addr_hex_roundtrip(void) {
  uint8_t a[8] = {0x28, 0xFF, 0x64, 0x1E, 0x0B, 0x00, 0x00, 0xAB};
  char hex[17];
  hcs::ow_addr_to_hex(a, hex);
  TEST_ASSERT_EQUAL_STRING("28FF641E0B0000AB", hex);
  uint8_t b[8];
  TEST_ASSERT_TRUE(hcs::ow_hex_to_addr(hex, b));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(a, b, 8);
}

void test_ow_hex_rejects_bad_input(void) {
  uint8_t out[8];
  TEST_ASSERT_FALSE(hcs::ow_hex_to_addr(nullptr, out));
  TEST_ASSERT_FALSE(hcs::ow_hex_to_addr("28FF", out));              // too short
  TEST_ASSERT_FALSE(hcs::ow_hex_to_addr("28FF641E0B0000ABX", out)); // too long
  TEST_ASSERT_FALSE(hcs::ow_hex_to_addr("28FF641E0B0000Ag", out));  // bad nibble
}

void test_ow_role_parse(void) {
  hcs::OwRole r;
  TEST_ASSERT_TRUE(hcs::ow_role_from_name("none", &r));
  TEST_ASSERT_EQUAL_INT(hcs::OW_ROLE_NONE, r);
  TEST_ASSERT_TRUE(hcs::ow_role_from_name("outdoor", &r));
  TEST_ASSERT_EQUAL_INT(hcs::OW_ROLE_OUTDOOR, r);
  TEST_ASSERT_TRUE(hcs::ow_role_from_name("return", &r));
  TEST_ASSERT_EQUAL_INT(hcs::OW_ROLE_RETURN, r);
  TEST_ASSERT_TRUE(hcs::ow_role_from_name("OUTDOOR", &r));
  TEST_ASSERT_EQUAL_INT(hcs::OW_ROLE_OUTDOOR, r);
  TEST_ASSERT_TRUE(hcs::ow_role_from_name("custom", &r));
  TEST_ASSERT_EQUAL_INT(hcs::OW_ROLE_CUSTOM, r);
  TEST_ASSERT_TRUE(hcs::ow_role_from_name("x", &r));
  TEST_ASSERT_EQUAL_INT(hcs::OW_ROLE_CUSTOM, r);
  TEST_ASSERT_FALSE(hcs::ow_role_from_name("banana", &r));
  TEST_ASSERT_FALSE(hcs::ow_role_from_name(nullptr, &r));
}

void test_ow_sanitize_name(void) {
  char out[hcs::kOwNameMax + 1];
  TEST_ASSERT_TRUE(hcs::ow_sanitize_name("Hall Temp", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("hall_temp", out);
  TEST_ASSERT_TRUE(hcs::ow_sanitize_name("A1", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("a1", out);
  TEST_ASSERT_FALSE(hcs::ow_sanitize_name("1bad", out, sizeof(out)));  // leading digit
  TEST_ASSERT_FALSE(hcs::ow_sanitize_name("x", out, sizeof(out)));     // too short
  TEST_ASSERT_FALSE(hcs::ow_sanitize_name("", out, sizeof(out)));
  TEST_ASSERT_FALSE(hcs::ow_sanitize_name(nullptr, out, sizeof(out)));
}

void test_ow_classify_health(void) {
  TEST_ASSERT_EQUAL_INT(hcs::OW_HEALTH_DISCONNECTED,
                        hcs::ow_classify(-127.0f, false, 0));
  TEST_ASSERT_EQUAL_INT(hcs::OW_HEALTH_IMPLAUSIBLE,
                        hcs::ow_classify(130.0f, false, 0));
  TEST_ASSERT_EQUAL_INT(hcs::OW_HEALTH_OK,
                        hcs::ow_classify(21.5f, false, 0));
  // stuck at power-on default
  TEST_ASSERT_EQUAL_INT(hcs::OW_HEALTH_STUCK85,
                        hcs::ow_classify(85.0f, true, 85.0f));
  // first 85 is still OK (could be real hot water)
  TEST_ASSERT_EQUAL_INT(hcs::OW_HEALTH_OK,
                        hcs::ow_classify(85.0f, false, 0));
  TEST_ASSERT_EQUAL_INT(hcs::OW_HEALTH_UNSTABLE,
                        hcs::ow_classify(40.0f, true, 20.0f));
}

void test_ow_assign_roles_and_custom(void) {
  hcs::OwSlot slots[hcs::kOwMaxSlots] = {};
  const char* a1 = "28FF641E0B0000AB";
  const char* a2 = "28FF641E0B0000CD";
  TEST_ASSERT_TRUE(hcs::ow_assign(slots, hcs::kOwMaxSlots, a1,
                                  hcs::OW_ROLE_OUTDOOR, nullptr));
  TEST_ASSERT_EQUAL_STRING(a1, hcs::ow_addr_for_role(
      slots, hcs::kOwMaxSlots, hcs::OW_ROLE_OUTDOOR));
  // steal outdoor to a2
  TEST_ASSERT_TRUE(hcs::ow_assign(slots, hcs::kOwMaxSlots, a2,
                                  hcs::OW_ROLE_OUTDOOR, nullptr));
  TEST_ASSERT_EQUAL_STRING(a2, hcs::ow_addr_for_role(
      slots, hcs::kOwMaxSlots, hcs::OW_ROLE_OUTDOOR));
  // custom needs name
  TEST_ASSERT_FALSE(hcs::ow_assign(slots, hcs::kOwMaxSlots, a1,
                                   hcs::OW_ROLE_CUSTOM, ""));
  TEST_ASSERT_TRUE(hcs::ow_assign(slots, hcs::kOwMaxSlots, a1,
                                  hcs::OW_ROLE_CUSTOM, "Hall Temp"));
  int idx = hcs::ow_slot_find(slots, hcs::kOwMaxSlots, a1);
  TEST_ASSERT_TRUE(idx >= 0);
  TEST_ASSERT_EQUAL_INT(hcs::OW_ROLE_CUSTOM, slots[idx].role);
  TEST_ASSERT_EQUAL_STRING("hall_temp", slots[idx].name);
  // duplicate custom name rejected
  TEST_ASSERT_FALSE(hcs::ow_assign(slots, hcs::kOwMaxSlots, a2,
                                   hcs::OW_ROLE_CUSTOM, "hall_temp"));
  // clear
  TEST_ASSERT_TRUE(hcs::ow_assign(slots, hcs::kOwMaxSlots, a1,
                                  hcs::OW_ROLE_NONE, nullptr));
  TEST_ASSERT_EQUAL_INT(-1, hcs::ow_slot_find(slots, hcs::kOwMaxSlots, a1));
}

void test_ow_legacy_migration(void) {
  hcs::OwSlot slots[hcs::kOwMaxSlots] = {};
  uint8_t n = hcs::ow_slots_from_legacy(
      "28FF641E0B0000AB", "28FF641E0B0000CD", slots, hcs::kOwMaxSlots);
  TEST_ASSERT_EQUAL_UINT8(2, n);
  TEST_ASSERT_EQUAL_STRING("28FF641E0B0000AB",
      hcs::ow_addr_for_role(slots, hcs::kOwMaxSlots, hcs::OW_ROLE_OUTDOOR));
  TEST_ASSERT_EQUAL_STRING("28FF641E0B0000CD",
      hcs::ow_addr_for_role(slots, hcs::kOwMaxSlots, hcs::OW_ROLE_RETURN));
}

void test_sensor_override_rules(void) {
  // assigned + fresh wins over OpenTherm
  hcs::TempValue v = hcs::resolve_temp(true, true, 5.5f, true, 3.2f);
  TEST_ASSERT_TRUE(v.valid);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.5f, v.celsius);

  // assigned but stale -> OpenTherm passes through
  v = hcs::resolve_temp(true, false, 5.5f, true, 3.2f);
  TEST_ASSERT_TRUE(v.valid);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.2f, v.celsius);

  // not assigned -> OpenTherm
  v = hcs::resolve_temp(false, true, 5.5f, true, 3.2f);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.2f, v.celsius);

  // nothing anywhere -> invalid (UI shows dash)
  v = hcs::resolve_temp(true, false, NAN, false, NAN);
  TEST_ASSERT_FALSE(v.valid);
}

void test_sensor_freshness_window(void) {
  TEST_ASSERT_TRUE(hcs::ow_reading_fresh(true, 0, 90000));
  TEST_ASSERT_TRUE(hcs::ow_reading_fresh(true, 90000, 90000));
  TEST_ASSERT_FALSE(hcs::ow_reading_fresh(true, 90001, 90000));
  TEST_ASSERT_FALSE(hcs::ow_reading_fresh(false, 0, 90000));
}

// ---------- boiler diagnostics text ----------
void test_boiler_diag_no_data_and_healthy(void) {
  char txt[160];
  hcs::BoilerDiag d;  // nothing seen yet
  TEST_ASSERT_EQUAL_INT(0, hcs::boiler_diag_text(d, txt, sizeof(txt)));
  TEST_ASSERT_EQUAL_STRING("no data", txt);
  TEST_ASSERT_EQUAL_STRING("unknown", hcs::boiler_diag_state(d));

  d.valid_asf = true;
  d.asf = 0;
  d.valid_oem = true;
  d.oem = 0;
  TEST_ASSERT_EQUAL_INT(0, hcs::boiler_diag_text(d, txt, sizeof(txt)));
  TEST_ASSERT_EQUAL_STRING("no faults", txt);
  TEST_ASSERT_EQUAL_STRING("ok", hcs::boiler_diag_state(d));
  TEST_ASSERT_FALSE(hcs::boiler_has_fault(d));
}

void test_boiler_diag_asf_bits(void) {
  char txt[160];
  hcs::BoilerDiag d;
  d.valid_asf = true;
  d.asf = 0x04;  // low water pressure
  TEST_ASSERT_EQUAL_INT(1, hcs::boiler_diag_text(d, txt, sizeof(txt)));
  TEST_ASSERT_EQUAL_STRING("low water pressure", txt);
  TEST_ASSERT_EQUAL_STRING("fault", hcs::boiler_diag_state(d));

  d.asf = 0x03;  // service request + lockout
  TEST_ASSERT_EQUAL_INT(2, hcs::boiler_diag_text(d, txt, sizeof(txt)));
  TEST_ASSERT_EQUAL_STRING("service request; lockout - reset required", txt);
}

void test_boiler_diag_oem_code(void) {
  char txt[160];
  hcs::BoilerDiag d;
  d.valid_oem = true;
  d.oem = 217;
  TEST_ASSERT_EQUAL_INT(1, hcs::boiler_diag_text(d, txt, sizeof(txt)));
  TEST_ASSERT_EQUAL_STRING("boiler diagnostic code 217", txt);

  // combined ASF + OEM
  d.valid_asf = true;
  d.asf = 0x08;
  TEST_ASSERT_EQUAL_INT(2, hcs::boiler_diag_text(d, txt, sizeof(txt)));
  TEST_ASSERT_EQUAL_STRING(
      "gas or flame fault; boiler diagnostic code 217", txt);
}

void test_boiler_diag_truncated_buffer_is_safe(void) {
  char tiny[12];
  hcs::BoilerDiag d;
  d.valid_asf = true;
  d.asf = 0xFF;  // many labels -> must not overflow
  hcs::boiler_diag_text(d, tiny, sizeof(tiny));
  TEST_ASSERT_TRUE(strlen(tiny) < sizeof(tiny));
}

// ---------- boiler capabilities ----------
void test_ot_clamp_bounds(void) {
  // known bounds enforced
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 55.0f,
      hcs::ot_clamp_with_bounds(80.0f, true, 0, 55, 10, 90));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 20.0f,
      hcs::ot_clamp_with_bounds(20.0f, true, 0, 55, 10, 90));
  // unknown bounds -> defaults; 80 is inside [10,90] so passes through
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 80.0f,
      hcs::ot_clamp_with_bounds(80.0f, false, 0, 55, 10, 90));
}

void test_ot_slow_read_round_robin(void) {
  // first seven cycles cover pressure, cfgs, capacity, bounds, fhb
  TEST_ASSERT_EQUAL_UINT16(18, hcs::ot_slow_read_id(0));
  TEST_ASSERT_EQUAL_UINT16(3, hcs::ot_slow_read_id(1));
  TEST_ASSERT_EQUAL_UINT16(2, hcs::ot_slow_read_id(2));
  TEST_ASSERT_EQUAL_UINT16(15, hcs::ot_slow_read_id(3));
  TEST_ASSERT_EQUAL_UINT16(48, hcs::ot_slow_read_id(4));
  TEST_ASSERT_EQUAL_UINT16(49, hcs::ot_slow_read_id(5));
  TEST_ASSERT_EQUAL_UINT16(13, hcs::ot_slow_read_id(6));
  TEST_ASSERT_EQUAL_UINT16(18, hcs::ot_slow_read_id(7));   // wraps
  TEST_ASSERT_EQUAL_UINT16(18, hcs::ot_slow_read_id(1001)); // 1001 %% 7 == 0
}

void test_ot_rbp_dhw_write_flag(void) {
  TEST_ASSERT_TRUE(hcs::ot_rbp_dhw_write_enabled(0x10));
  TEST_ASSERT_TRUE(hcs::ot_rbp_dhw_write_enabled(0xF0));
  TEST_ASSERT_FALSE(hcs::ot_rbp_dhw_write_enabled(0x00));
  TEST_ASSERT_FALSE(hcs::ot_rbp_dhw_write_enabled(0x0F));
}

void test_ot_fhb_format(void) {
  uint8_t codes[] = {0x01, 0xAB, 0x05};
  char out[32];
  hcs::ot_fhb_format(codes, 3, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("01 AB 05", out);
  hcs::ot_fhb_format(codes, 0, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("", out);
  // truncation safety
  char tiny[5];
  hcs::ot_fhb_format(codes, 3, tiny, sizeof(tiny));
  TEST_ASSERT_TRUE(strlen(tiny) < sizeof(tiny));
}

// ---------- connection-loss failsafe policy ----------
void test_fs_states(void) {
  using hcs::FsState;
  // link up -> connected regardless
  TEST_ASSERT_EQUAL_INT((int)FsState::CONNECTED,
      (int)hcs::fs_evaluate(true, true, 0, 600000));
  // disabled owner switch -> never failsafe
  TEST_ASSERT_EQUAL_INT((int)FsState::CONNECTED,
      (int)hcs::fs_evaluate(false, false, 999999999UL, 600000));
  // inside grace -> hold
  TEST_ASSERT_EQUAL_INT((int)FsState::HOLD,
      (int)hcs::fs_evaluate(true, false, 599999, 600000));
  // beyond grace -> failsafe
  TEST_ASSERT_EQUAL_INT((int)FsState::FAILSAFE,
      (int)hcs::fs_evaluate(true, false, 600000, 600000));
}

void test_fs_ch_demand(void) {
  // freeze protection forces heat in failsafe
  TEST_ASSERT_TRUE(hcs::fs_ch_demand(hcs::FsState::FAILSAFE, false));
  // otherwise last command passes through
  TEST_ASSERT_FALSE(hcs::fs_ch_demand(hcs::FsState::CONNECTED, false));
  TEST_ASSERT_TRUE(hcs::fs_ch_demand(hcs::FsState::HOLD, true));
  TEST_ASSERT_TRUE(hcs::fs_ch_demand(hcs::FsState::CONNECTED, true));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_ch_enable_on_variants);
  RUN_TEST(test_ch_enable_off_variants);
  RUN_TEST(test_otgw_chenable_alias);
  RUN_TEST(test_dhw_enable_roundtrip);
  RUN_TEST(test_flow_setpoint_floats);
  RUN_TEST(test_flow_setpoint_garbage_is_zero);
  RUN_TEST(test_max_modulation_clamped);
  RUN_TEST(test_reboot_and_ota_url);
  RUN_TEST(test_unknown_topic_ignored);
  RUN_TEST(test_prefix_must_not_leak_into_other_commands);
  RUN_TEST(test_null_inputs_safe);
  RUN_TEST(test_wc_disabled_is_nan);
  RUN_TEST(test_wc_nan_outdoor_is_nan);
  RUN_TEST(test_wc_at_reference_returns_flow_min);
  RUN_TEST(test_wc_above_reference_clamps_to_flow_min);
  RUN_TEST(test_wc_at_design_returns_flow_max);
  RUN_TEST(test_wc_below_design_clamps_to_flow_max);
  RUN_TEST(test_wc_midpoint_interpolates);
  RUN_TEST(test_wc_quarter_point_interpolates);
  RUN_TEST(test_wc_cfg_parse_valid);
  RUN_TEST(test_wc_cfg_parse_rejects_garbage);
  RUN_TEST(test_wc_cfg_parse_rejects_contradictory);
  RUN_TEST(test_wc_cfg_design_below_reference_is_valid);
  RUN_TEST(test_wc_cfg_failure_leaves_struct_untouched);
  RUN_TEST(test_gw_forward_passthrough_by_default);
  RUN_TEST(test_gw_setpoint_override_rewrites_tset);
  RUN_TEST(test_gw_override_ignores_other_ids_and_reads);
  RUN_TEST(test_gw_override_disable_with_nan);
  RUN_TEST(test_gw_link_down_answers_local_from_cache);
  RUN_TEST(test_gw_link_down_unknown_id_is_unsupported_local);
  RUN_TEST(test_gw_cache_skips_invalid_responses);
  RUN_TEST(test_gw_link_up_always_forwards_even_if_cached);
  RUN_TEST(test_gw_f88_roundtrip);
  RUN_TEST(test_gw_set_mode_parse);
  RUN_TEST(test_gw_autodetect_undecided_before_window);
  RUN_TEST(test_gw_autodetect_master_when_silent);
  RUN_TEST(test_gw_autodetect_gateway_with_two_frames);
  RUN_TEST(test_gw_autodetect_custom_window);
  RUN_TEST(test_ow_addr_hex_roundtrip);
  RUN_TEST(test_ow_hex_rejects_bad_input);
  RUN_TEST(test_ow_role_parse);
  RUN_TEST(test_ow_sanitize_name);
  RUN_TEST(test_ow_classify_health);
  RUN_TEST(test_ow_assign_roles_and_custom);
  RUN_TEST(test_ow_legacy_migration);
  RUN_TEST(test_sensor_override_rules);
  RUN_TEST(test_sensor_freshness_window);
  RUN_TEST(test_boiler_diag_no_data_and_healthy);
  RUN_TEST(test_boiler_diag_asf_bits);
  RUN_TEST(test_boiler_diag_oem_code);
  RUN_TEST(test_boiler_diag_truncated_buffer_is_safe);
  RUN_TEST(test_ot_clamp_bounds);
  RUN_TEST(test_ot_slow_read_round_robin);
  RUN_TEST(test_ot_rbp_dhw_write_flag);
  RUN_TEST(test_ot_fhb_format);
  RUN_TEST(test_fs_states);
  RUN_TEST(test_fs_ch_demand);
  RUN_TEST(test_gw_override_parse);
  RUN_TEST(test_gw_topics_do_not_shadow_flow_setpoint);
  return UNITY_END();
}
