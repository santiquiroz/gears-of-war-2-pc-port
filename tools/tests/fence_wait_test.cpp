#include <rex/ui/d3d12/gow2_fence_wait.h>
#include <array>
#include <cstdio>
#include <string>
using namespace rex::ui::d3d12::fence_wait;
struct Fake {
  std::array<uint64_t, 2> values{0, 10};
  int reads = 0, arms = 0, waits = 0, signals = 0;
  int32_t signal_hr = 0, event_hr = 0;
  uint32_t wait_result = 0;
  std::string order;
  Result run(bool signal = false, uint64_t target = 10) {
    auto read = [&] {
      order += 'R';
      return values[reads++ ? 1 : 0];
    };
    auto arm = [&](uint64_t value) {
      order += 'A';
      ++arms;
      return value == target ? event_hr : int32_t(-1);
    };
    auto wait = [&] {
      order += 'W';
      ++waits;
      return wait_result;
    };
    if (signal)
      return SignalAndAwait(
          target,
          [&](uint64_t value) {
            order += 'S';
            ++signals;
            return value == target ? signal_hr : int32_t(-1);
          },
          read, arm, wait);
    return Await(target, read, arm, wait);
  }
};
int main() {
  int errors = 0, count = 0;
  auto check = [&](bool ok, const char* label) {
    ++count;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok)
      ++errors;
  };
  {
    Fake f;
    f.values = {10, 10};
    auto r = f.run();
    check(bool(r) && r.completed == 10 && f.order == "R",
          "already completed fence requires no event/wait");
  }
  {
    Fake f;
    auto r = f.run();
    check(bool(r) && r.completed == 10 && f.order == "RAWR",
          "pending fence arms event before wait and rechecks completion");
  }
  {
    Fake f;
    f.values = {UINT64_MAX, 10};
    auto r = f.run();
    check(r.status == Status::kDeviceRemoved && f.order == "R",
          "initial removed sentinel stops before wait/reclamation");
  }
  {
    Fake f;
    f.values = {0, UINT64_MAX};
    auto r = f.run();
    uint64_t watermark = 7;
    if (r)
      watermark = r.completed;
    check(r.status == Status::kDeviceRemoved && watermark == 7 && f.order == "RAWR",
          "post-wait removed sentinel never publishes completed watermark");
  }
  {
    Fake f;
    f.event_hr = int32_t(0x80070006u);
    auto r = f.run();
    check(r.status == Status::kEventFailed && r.error == 0x80070006u && f.order == "RA",
          "event HRESULT failure preserves error and skips wait");
  }
  {
    Fake f;
    f.wait_result = 0xffffffffu;
    auto r = f.run();
    check(r.status == Status::kWaitFailed && r.error == 0xffffffffu && f.reads == 1,
          "failed OS wait does not accept/read completion");
  }
  {
    Fake f;
    f.wait_result = 258;
    auto r = f.run();
    check(r.status == Status::kWaitFailed && r.error == 258 && f.reads == 1,
          "timeout is not treated as completed work");
  }
  {
    Fake f;
    f.values = {0, 9};
    auto r = f.run();
    check(r.status == Status::kIncomplete && !r,
          "signaled event without target completion fails closed");
  }
  {
    Fake f;
    f.signal_hr = int32_t(0x887a0005u);
    auto r = f.run(true);
    check(r.status == Status::kSignalFailed && r.error == 0x887a0005u && f.order == "S",
          "queue Signal failure skips event and wait");
  }
  {
    Fake f;
    auto r = f.run(true);
    check(bool(r) && f.order == "SRAWR" && f.signals == 1 && f.arms == 1,
          "S_OK zero Signal still reaches SetEventOnCompletion");
  }
  {
    Fake f;
    f.values = {10, 10};
    auto r = f.run(true);
    check(bool(r) && f.order == "SR", "queue already complete after Signal requires no event");
  }
  {
    Fake f;
    f.values = {UINT64_MAX, 10};
    auto r = f.run(true);
    check(r.status == Status::kDeviceRemoved && f.order == "SR",
          "removed out-of-submission fence cannot report success");
  }
  {
    Fake f;
    f.event_hr = int32_t(0x80004005u);
    auto r = f.run(true);
    check(r.status == Status::kEventFailed && f.order == "SRA",
          "out-of-submission event failure stops before wait");
  }
  {
    Fake f;
    f.values = {0, UINT64_MAX};
    auto r = f.run(true);
    check(r.status == Status::kDeviceRemoved && f.order == "SRAWR",
          "out-of-submission loss during wait is recognized");
  }
  {
    Fake f;
    auto r = f.run(false, 0);
    check(bool(r) && r.completed == 0 && f.order == "R",
          "poll with target zero never stalls normal submissions");
  }
  std::printf("%d checks, %d failures\n", count, errors);
  return errors ? 1 : 0;
}
