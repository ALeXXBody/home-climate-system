#pragma once
/**
 * Single-writer/single-reader ring buffer for latched OpenTherm master
 * requests (portable, host-testable).
 *
 * Why: the old slave latch was a single slot (req_ + pending_ flag). A
 * master burst — e.g. CH setpoint write immediately followed by a status
 * read — overwrote the un-serviced slot and the older request was silently
 * dropped until the next poll round. The ring keeps the last N requests;
 * when full it drops the NEWEST (the master re-issues short-cycle reads;
 * stale writes must go first, and the ISR never touches the reader side,
 * which keeps the SPSC invariant trivially intact).
 */

#include <stddef.h>
#include <stdint.h>

namespace hcs {

class OtFifo {
 public:
  static constexpr uint8_t kCapacity = 8;  // power of two
  static constexpr uint8_t kMask = kCapacity - 1;

  /** Wake from any context: drop queued items, restart empty. */
  void reset() {
    head_ = 0;
    tail_ = 0;
    drops_ = 0;
  }

  /** ISR-context writer. Returns false (and counts a drop) when full. */
  inline bool push(uint32_t value) volatile {
    uint8_t next = (uint8_t)((head_ + 1) & kMask);
    if (next == tail_) {
      drops_++;
      return false;
    }
    buf_[head_] = value;
    head_ = next;
    return true;
  }

  /** Loop-context reader. Returns false when empty. */
  inline bool pop(uint32_t& out) volatile {
    if (head_ == tail_) return false;
    out = buf_[tail_];
    tail_ = (uint8_t)((tail_ + 1) & kMask);
    return true;
  }

  /** Approximate items waiting (ISR may push concurrently). */
  inline uint8_t items() const volatile {
    return (uint8_t)((head_ - tail_) & kMask);
  }

  /** Requests the ring was forced to reject (full). */
  uint32_t drops() const volatile { return drops_; }

 private:
  volatile uint32_t buf_[kCapacity];
  volatile uint8_t head_ = 0;  // written by the writer, read by both
  volatile uint8_t tail_ = 0;  // written by the reader
  volatile uint32_t drops_ = 0;
  static_assert((kCapacity & (kCapacity - 1)) == 0, "power of two");
};

}  // namespace hcs
