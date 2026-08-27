#pragma once
/**
 * In-RAM system log — last N lines for the web "Log" tab and Serial.
 * Survives until reboot; survives brief HTTP wedges so you can still see
 * why the board rebooted after the next boot (boot reason is logged first).
 */
#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>

namespace hcs {

class SysLog {
 public:
  static constexpr uint8_t CAPACITY = 80;
  static constexpr uint8_t LINE_LEN = 120;

  void clear() {
    head_ = 0;
    count_ = 0;
  }

  void log(const char* tag, const char* fmt, ...) {
    char body[LINE_LEN - 24];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(body, sizeof(body), fmt, ap);
    va_end(ap);

    char line[LINE_LEN];
    unsigned long s = millis() / 1000UL;
    snprintf(line, sizeof(line), "[%5lu.%02lus] %-6s %s",
             s, (millis() % 1000UL) / 10UL, tag ? tag : "sys", body);

    entries_[head_][0] = '\0';
    strlcpy(entries_[head_], line, LINE_LEN);
    head_ = (head_ + 1) % CAPACITY;
    if (count_ < CAPACITY) count_++;

    Serial.println(line);
  }

  uint8_t count() const { return count_; }

  /** oldest_index 0 = oldest retained … count()-1 = newest */
  const char* line(uint8_t oldest_index) const {
    if (oldest_index >= count_) return "";
    uint8_t start = (CAPACITY + head_ - count_) % CAPACITY;
    return entries_[(start + oldest_index) % CAPACITY];
  }

 private:
  char entries_[CAPACITY][LINE_LEN] = {};
  uint8_t head_ = 0;
  uint8_t count_ = 0;
};

/** Global instance — defined in net_services.cpp */
extern SysLog g_log;

}  // namespace hcs

// Convenience macros
#define HCS_LOG(tag, ...) ::hcs::g_log.log(tag, __VA_ARGS__)
