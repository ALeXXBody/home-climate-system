#include <unity.h>
#include "hcs_commands.h"

void setUp(void) {}
void tearDown(void) {}

static HcsCommandResult parse(const char* topic, const char* payload) {
  return hcs_parse_command(topic, payload);
}

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
  return UNITY_END();
}
