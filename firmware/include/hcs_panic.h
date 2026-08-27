#pragma once
/**
 * Panic breadcrumbs — last "where was I" tag in RTC memory so the next
 * boot can report it after ESP_RST_PANIC. Safe to call from loop; do not
 * call from ISRs.
 */
#include <Arduino.h>

namespace hcs {

#if defined(ESP32)
void panic_mark(const char* tag);
/** Returns sticky tag from previous run if last reset was PANIC; else "". */
const char* panic_last_tag();
void panic_clear();
#else
inline void panic_mark(const char*) {}
inline const char* panic_last_tag() { return ""; }
inline void panic_clear() {}
#endif

}  // namespace hcs

#define HCS_MARK(tag) ::hcs::panic_mark(tag)
