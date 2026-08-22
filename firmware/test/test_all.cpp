#include <math.h>
#include <initializer_list>
#include <unity.h>
#include "hcs_commands.h"
#include "hcs_gateway.h"
#include "hcs_weather_comp.h"

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
  TEST_ASSERT_TRUE(gw.bool_value);

  HcsCommandResult mo = parse("hcs/n/set/gw/set_mode", "master_only");
  TEST_ASSERT_EQUAL(HCS_CMD_GW_MODE, mo.cmd);
  TEST_ASSERT_FALSE(mo.bool_value);

  HcsCommandResult junk = parse("hcs/n/set/gw/set_mode", "banana");
  TEST_ASSERT_EQUAL(HCS_CMD_GW_MODE, junk.cmd);
  TEST_ASSERT_FALSE(junk.bool_value);  // anything unknown -> master_only
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
  RUN_TEST(test_gw_override_parse);
  RUN_TEST(test_gw_topics_do_not_shadow_flow_setpoint);
  return UNITY_END();
}
