// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// The guest D3D's GPU wait poll: sleep between polls while D3D Swap waits for queued swaps, an
// optional shorter swap queue, and GPUWAIT statistics.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc.h>

#include "gow2_fps_cap.h"
#include "gow2_guest.h"

REXCVAR_DEFINE_INT32(gow2_gpu_wait_sleep_us, 0, "GameFixes",
                     "Microseconds the render thread sleeps between polls while it waits for the GPU "
                     "command thread (ring buffer space or queued frames); 0 = spin like the Xbox 360 "
                     "code. Takes effect at once.")
    .range(0, 5000);

REXCVAR_DEFINE_INT32(gow2_gpu_wait_spin_us, 0, "GameFixes",
                     "Microseconds each wait for the GPU command thread spins before it starts "
                     "sleeping between polls (gow2_gpu_wait_sleep_us). Takes effect at once.")
    .range(0, 5000);

REXCVAR_DEFINE_INT32(gow2_swap_queue_limit, 0, "GameFixes",
                     "Frames D3D Swap lets the render thread queue ahead of the GPU command thread; "
                     "0 = the game's 15, otherwise at least 2. Takes effect at once.")
    .range(0, 15);

REXCVAR_DEFINE_BOOL(gow2_gpu_wait_stats, false, "GameFixes",
                    "Log a GPUWAIT line every five seconds: the guest D3D's GPU waits and how many "
                    "frames were queued when Swap was called.");

REX_EXTERN(__imp__sub_82977688);

namespace rex::glue {
namespace {

constexpr uint32_t kSwapWaitReturn = 0x8298DB80;  // in sub_8298D430, D3D Swap
constexpr uint32_t kRingWaitReturn = 0x8298C360;  // in sub_8298C2A0, waits for ring buffer space
constexpr uint32_t kReadPointerSlotOffset = 10896;
constexpr uint32_t kWaitSeenReadPointerOffset = 8;
constexpr uint32_t kGameSwapQueueLimit = 15;
constexpr uint32_t kMinSwapQueueLimit = 2;
constexpr uint32_t kSwapsIssuedOffset = 16560;
constexpr uint32_t kSwapsDoneOffset = 16568;
constexpr int64_t kMaxRingWaitNs = 1'000'000;
constexpr int64_t kStatsPeriodNs = 5'000'000'000;

struct Stats {
  std::atomic<uint64_t> swap_calls{0};
  std::atomic<uint64_t> swap_waits{0};
  std::atomic<uint64_t> swap_wait_ns{0};
  std::atomic<uint64_t> swap_queued_sum{0};
  std::atomic<uint32_t> swap_queued_max{0};
  std::atomic<uint64_t> ring_calls{0};
  std::atomic<uint64_t> ring_ns{0};
  std::atomic<uint64_t> other_polls{0};
  std::atomic<uint64_t> other_ns{0};
};

Stats stats;
std::atomic<int64_t> stats_deadline{0};

int64_t NowNs() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

uint32_t QueuedSwaps(const uint8_t* base, uint32_t wait) {
  const uint32_t device = guest::LoadU32(base, wait);
  return guest::LoadU32(base, device + kSwapsIssuedOffset) -
         guest::LoadU32(base, device + kSwapsDoneOffset);
}

// At least 2: when Swap waits, its own frame may not have reached the GPU command thread yet.
uint32_t SwapQueueLimit() {
  const int32_t limit = REXCVAR_GET(gow2_swap_queue_limit);
  if (limit <= 0) return kGameSwapQueueLimit;
  return std::max(static_cast<uint32_t>(limit), kMinSwapQueueLimit);
}

void SleepBetweenPolls(int64_t start) {
  const int32_t sleep_us = REXCVAR_GET(gow2_gpu_wait_sleep_us);
  if (sleep_us <= 0) return;
  const int32_t spin_us = REXCVAR_GET(gow2_gpu_wait_spin_us);
  if (spin_us > 0 && NowNs() - start < static_cast<int64_t>(spin_us) * 1000) return;
  BulkSleepNanos(static_cast<int64_t>(sleep_us) * 1000);
}

void Poll(PPCContext& ctx, uint8_t* base, uint32_t caller, uint32_t wait) {
  ctx.lr = caller;
  ctx.r3.u64 = wait;
  __imp__sub_82977688(ctx, base);
}

void Add(std::atomic<uint64_t>& counter, uint64_t value) {
  counter.fetch_add(value, std::memory_order_relaxed);
}

uint64_t Take(std::atomic<uint64_t>& counter) {
  return counter.exchange(0, std::memory_order_relaxed);
}

void RaiseMax(std::atomic<uint32_t>& max, uint32_t value) {
  uint32_t seen = max.load(std::memory_order_relaxed);
  while (value > seen && !max.compare_exchange_weak(seen, value, std::memory_order_relaxed)) {
  }
}

void RecordSwap(uint32_t queued, bool waited, int64_t ns) {
  Add(stats.swap_calls, 1);
  Add(stats.swap_queued_sum, queued);
  RaiseMax(stats.swap_queued_max, queued);
  if (!waited) return;
  Add(stats.swap_waits, 1);
  Add(stats.swap_wait_ns, static_cast<uint64_t>(ns));
}

void RecordRing(int64_t ns) {
  Add(stats.ring_calls, 1);
  Add(stats.ring_ns, static_cast<uint64_t>(ns));
}

void RecordPoll(int64_t ns) {
  Add(stats.other_polls, 1);
  Add(stats.other_ns, static_cast<uint64_t>(ns));
}

void LogStats() {
  const uint64_t swaps = Take(stats.swap_calls);
  const double queued_avg = double(Take(stats.swap_queued_sum)) / double(swaps > 0 ? swaps : 1);
  REXLOG_INFO("GPUWAIT ring_calls={} ring_ms={:.1f} swaps={} swap_waited={} swap_ms={:.1f} "
              "queued_avg={:.2f} queued_max={} limit={} sleep_us={} spin_us={} other_polls={} "
              "other_ms={:.1f}",
              Take(stats.ring_calls), double(Take(stats.ring_ns)) / 1e6, swaps,
              Take(stats.swap_waits), double(Take(stats.swap_wait_ns)) / 1e6, queued_avg,
              stats.swap_queued_max.exchange(0, std::memory_order_relaxed), SwapQueueLimit(),
              REXCVAR_GET(gow2_gpu_wait_sleep_us), REXCVAR_GET(gow2_gpu_wait_spin_us),
              Take(stats.other_polls), double(Take(stats.other_ns)) / 1e6);
}

// The first call only arms the deadline, so every line covers a whole period.
void MaybeLogStats(int64_t now) {
  int64_t deadline = stats_deadline.load(std::memory_order_relaxed);
  if (now < deadline) return;
  if (!stats_deadline.compare_exchange_strong(deadline, now + kStatsPeriodNs,
                                              std::memory_order_relaxed)) {
    return;
  }
  if (deadline != 0) LogStats();
}

// The caller re-checks the queue after the call, finds it under 15 and leaves its loop; r3 = 0
// from the poll's hang check still reaches it.
void SwapWait(PPCContext& ctx, uint8_t* base, uint32_t wait) {
  const uint32_t limit = SwapQueueLimit();
  const uint32_t queued = QueuedSwaps(base, wait);
  const int64_t start = NowNs();
  bool waited = false;
  for (;;) {
    Poll(ctx, base, kSwapWaitReturn, wait);
    if (ctx.r3.u32 == 0 || QueuedSwaps(base, wait) < limit) break;
    waited = true;
    SleepBetweenPolls(start);
  }
  if (!REXCVAR_GET(gow2_gpu_wait_stats)) return;
  const int64_t now = NowNs();
  RecordSwap(queued, waited, now - start);
  MaybeLogStats(now);
}

// The caller re-checks only after a poll, and its check reads nothing that changes while it waits
// except the written-back read pointer: until that moves from the value the poll saw last, the
// wait cannot end. Control still goes back to the caller at least every millisecond.
void RingWait(PPCContext& ctx, uint8_t* base, uint32_t wait) {
  const uint32_t device = guest::LoadU32(base, wait);
  const uint32_t slot = guest::LoadU32(base, device + kReadPointerSlotOffset);
  const uint32_t seen = guest::LoadU32(base, wait + kWaitSeenReadPointerOffset);
  const int64_t start = NowNs();
  for (;;) {
    Poll(ctx, base, kRingWaitReturn, wait);
    if (ctx.r3.u32 == 0 || guest::LoadU32(base, slot) != seen) break;
    if (NowNs() - start >= kMaxRingWaitNs) break;
    SleepBetweenPolls(start);
  }
  if (!REXCVAR_GET(gow2_gpu_wait_stats)) return;
  const int64_t now = NowNs();
  RecordRing(now - start);
  MaybeLogStats(now);
}

void TimedPoll(PPCContext& ctx, uint8_t* base, uint32_t caller, uint32_t wait) {
  const int64_t start = NowNs();
  Poll(ctx, base, caller, wait);
  const int64_t now = NowNs();
  RecordPoll(now - start);
  MaybeLogStats(now);
}

}  // namespace
}  // namespace rex::glue

REX_HOOK_RAW(sub_82977688) {
  const uint32_t caller = static_cast<uint32_t>(ctx.lr);  // the poll overwrites lr and r3
  const uint32_t wait = ctx.r3.u32;
  if (caller == rex::glue::kRingWaitReturn) {
    rex::glue::RingWait(ctx, base, wait);
  } else if (caller == rex::glue::kSwapWaitReturn) {
    rex::glue::SwapWait(ctx, base, wait);
  } else if (REXCVAR_GET(gow2_gpu_wait_stats)) {
    rex::glue::TimedPoll(ctx, base, caller, wait);
  } else {
    rex::glue::Poll(ctx, base, caller, wait);
  }
}
