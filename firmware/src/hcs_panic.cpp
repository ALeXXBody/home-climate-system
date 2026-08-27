#include "hcs_panic.h"

#if defined(ESP32)
#include <esp_system.h>
#include <string.h>

namespace hcs {
namespace {

constexpr uint32_t kMagic = 0x48435350u;  // 'HCSP'
RTC_NOINIT_ATTR uint32_t s_magic;
RTC_NOINIT_ATTR char s_tag[40];
RTC_NOINIT_ATTR uint32_t s_uptime_ms;
char s_report[48];
bool s_loaded = false;

}  // namespace

void panic_mark(const char* tag) {
  if (!tag) tag = "?";
  strncpy(s_tag, tag, sizeof(s_tag) - 1);
  s_tag[sizeof(s_tag) - 1] = '\0';
  s_uptime_ms = millis();
  s_magic = kMagic;
}

const char* panic_last_tag() {
  if (s_loaded) return s_report;
  s_loaded = true;
  s_report[0] = '\0';
  if (esp_reset_reason() != ESP_RST_PANIC) return s_report;
  if (s_magic != kMagic || s_tag[0] == '\0') {
    snprintf(s_report, sizeof(s_report), "(no mark)");
    return s_report;
  }
  snprintf(s_report, sizeof(s_report), "%s @%lums", s_tag,
           (unsigned long)s_uptime_ms);
  return s_report;
}

void panic_clear() {
  s_magic = 0;
  s_tag[0] = '\0';
}

}  // namespace hcs
#endif
