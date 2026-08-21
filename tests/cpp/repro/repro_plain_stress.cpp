// Plain-build (NO sanitizer) stress harness for the stale-ISurfaceManager*
// race reproduced deterministically by repro_sm_uaf.
//
// It was written to test whether that race alone can corrupt live VirtualDOM /
// ComponentSnapshot structures and surface as a DELAYED crash on the worker
// thread, rather than faulting immediately in the caller's frame. That was
// never observed: every fault landed on the calling thread. Keep it as an
// exploratory probe, not as evidence for any particular field crash.
//
// Scenario per round (block size measured on this platform: SurfaceManager
// make_shared block = 248 bytes requested / 256 size class, object at +24,
// enable_shared_from_this.__weak_this_ at object+8, so the control-block
// pointer used by shared_from_this() sits at block offset 40):
//   1. An "attacker" SurfaceManager is created; a feeder thread resolves
//      the RAW ISurfaceManager* exactly like the JNI layer does
//      (engine->findSurfaceManager) and then blocks (simulated preemption).
//   2. The main thread destroys the attacker. The uninit lambda runs on the
//      worker; when it finishes, the last shared_ptr reference drops and the
//      248-byte block is freed.
//   3. A "victim" SurfaceManager streams a real surface + many components.
//      The worker allocates tree nodes, map nodes, SerializableData Impls
//      and make_shared control blocks; some of them land IN the freed
//      attacker block (same size class), so the dangling pointer now
//      aliases live worker-thread structures.
//   4. The feeder resumes and calls the public APIs through the dangling
//      pointer:
//        - block still holds original bytes -> bad_weak_ptr, caught;
//        - block unmapped/garbage vtable -> direct fault, recovered via the
//          signal handler (feeder fault counter);
//        - block holds a live victim structure -> _isRunning reads a member
//          byte (often non-zero) and shared_from_this() reads a member
//          pointer as the control block, then performs an ATOMIC INCREMENT
//          (wild write) inside the live structure. Silent at this point.
//   5. The victim keeps streaming / updating / tearing down, so the worker
//      traverses the possibly-corrupted structures (updateChildren, snapshot
//      copies, ComponentManager/VirtualDOM destruction) — this is where a
//      delayed worker-thread crash would surface if the hypothesis held
//      (stacks in __tree destroy / ComponentSnapshot / SerializableData
//      refcount rather than receiveTextChunk). Not observed so far.
//
// Any crash is fatal for the process (by design): run this under a
// debugger (lldb -b -o run -k "bt all" -k quit) or inspect the OS crash
// report to obtain the stack.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <csetjmp>
#include <cstdio>
#include <cstdlib>
#include <execinfo.h>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "agenui_engine.h"
#include "agenui_engine_entry.h"
#include "agenui_logger_interface.h"
#include "agenui_surface_manager_interface.h"

namespace {

// Silent logger: per-round logging dominates wall time and drowns the crash
// output; the stress harness only cares about crashes.
class NullLogger : public agenui::IRuntimeLogger {
public:
    void log(agenui::LogLevel, const char*, const char*, int, const char*, ...) override {}
    agenui::LogLevel getMinLevel() const override { return agenui::LOG_LEVEL_FATAL; }
};

// Fault recovery for the racing (feeder) thread only. When the dangling
// call faults immediately (garbage vtable / unmapped garbage pointer), we
// recover and keep the process alive so wild writes can accumulate across
// rounds and eventually blow up inside worker-thread structures — the
// production shape. A fault on any other thread (i.e. the worker) is the
// observation we are hunting and must print + terminate.
thread_local sigjmp_buf g_feederJmp;
thread_local bool g_feederArmed = false;
std::atomic<long> g_feederFaults{0};
std::atomic<long> g_weakThrows{0};   // dangling call saw stale bytes (bad_weak_ptr)
std::atomic<long> g_silentWild{0};   // dangling call ran against reused memory

struct Gate {
    std::mutex m;
    std::condition_variable cv;
    bool open = false;
    void wait() {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [this] { return open; });
    }
    void release() {
        {
            std::lock_guard<std::mutex> lk(m);
            open = true;
        }
        cv.notify_all();
    }
    void reset() {
        std::lock_guard<std::mutex> lk(m);
        open = false;
    }
};

// Heap churn: allocate and free many chunks of assorted sizes so the
// allocator reuses the just-freed SurfaceManager block (mimics scudo on
// device). Without this the freed chunk usually still holds its original
// bytes and the race degenerates into bad_weak_ptr instead of wild writes.
void HeapChurn() {
    std::vector<std::unique_ptr<char[]>> hold;
    hold.reserve(256);
    static const size_t kSizes[] = {96, 128, 160, 192, 224, 248, 256, 320, 384, 512};
    for (int i = 0; i < 256; ++i) {
        size_t sz = kSizes[i % (sizeof(kSizes) / sizeof(kSizes[0]))];
        auto p = std::make_unique<char[]>(sz);
        p[0] = static_cast<char>(i);
        p[sz - 1] = static_cast<char>(i);
        if (i % 3 != 0) {
            hold.push_back(std::move(p));
        }
    }
    // Free in a scrambled order.
    for (int i = static_cast<int>(hold.size()) - 1; i >= 0; i -= 2) {
        hold[i].reset();
    }
    hold.clear();
}

std::string CreateSurfaceJson(const std::string& surfaceId) {
    return R"({"version":"v0.9","createSurface":{"surfaceId":")" + surfaceId +
           R"(","catalogId":"https://a2ui.org/specification/v0_9/standard_catalog.json","theme":{},"sendDataModel":false,"animated":true}})";
}

std::string UpdateComponentsJson(const std::string& surfaceId, int round) {
    // Many components with nested styles: maximizes worker-side allocations
    // (map/tree nodes, SerializableData Impls, control blocks) in the same
    // size class as the freed SurfaceManager block, so the allocator hands
    // that block out to live worker structures.
    std::string s = R"({"version":"v0.9","updateComponents":{"surfaceId":")" + surfaceId +
                    R"(","components":[)" +
                    R"({"id":"root","component":"Column","children":["t1","t2","t3","t4","t5","t6","t7","t8"]},)";
    for (int i = 1; i <= 8; ++i) {
        s += R"({"id":"t)" + std::to_string(i) +
             R"(","component":"Text","text":"hello-)" + std::to_string(round) + "-" +
             std::to_string(i) +
             R"(","style":{"width":"100%","padding":"8px","backgroundColor":"#fff"}},)";
    }
    s.pop_back();  // trailing comma
    s += R"(]}})";
    return s;
}

std::string DeleteSurfaceJson(const std::string& surfaceId) {
    return R"({"version":"v0.9","deleteSurface":{"surfaceId":")" + surfaceId + R"("}})";
}

}  // namespace

static void CrashHandler(int sig) {
    if (g_feederArmed) {
        // Fault inside the racing call: recover, keep the harness running.
        g_feederArmed = false;
        g_feederFaults.fetch_add(1);
        siglongjmp(g_feederJmp, 1);
    }
    const char msg[] = "\n=== STRESS CRASH: caught signal, backtrace: ===\n";
    (void)!write(STDERR_FILENO, msg, sizeof(msg) - 1);
    void* frames[64];
    int n = backtrace(frames, 64);
    backtrace_symbols_fd(frames, n, STDERR_FILENO);
    _exit(128 + sig);
}

int main(int argc, char** argv) {
    // Alternate signal stack: faults may corrupt the thread stack; the
    // handler must still run.
    static char altStackMem[64 * 1024];
    stack_t ss{};
    ss.ss_sp = altStackMem;
    ss.ss_size = sizeof(altStackMem);
    sigaltstack(&ss, nullptr);

    struct sigaction sa{};
    sa.sa_handler = CrashHandler;
    sa.sa_flags = SA_ONSTACK;
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);

    const int kRounds = (argc > 1) ? std::atoi(argv[1]) : 2000;

    agenui::IAGenUIEngine* engine = agenui::initAGenUIEngine();
    if (!engine) {
        std::fprintf(stderr, "stress: init failed\n");
        return 2;
    }
    static NullLogger nullLogger;
    engine->setRuntimeLogger(&nullLogger);

    std::printf("stress: rounds=%d\n", kRounds);

    for (int round = 0; round < kRounds; ++round) {
        // ---- 1. Victim exists but does not stream yet: its worker-side
        // allocations must happen AFTER the attacker block is freed so they
        // can reuse it.
        const std::string victimSurface = "victim-" + std::to_string(round);
        agenui::ISurfaceManager* victim = engine->createSurfaceManager();
        if (!victim) {
            break;
        }

        // ---- 2. Attacker: raw pointer acquired exactly like JNI does
        agenui::ISurfaceManager* attacker = engine->createSurfaceManager();
        if (!attacker) {
            engine->destroySurfaceManager(victim);
            break;
        }
        const int attackerId = attacker->getInstanceId();

        Gate pointerReady;
        Gate callerGate;
        std::thread feeder([engine, attackerId, &pointerReady, &callerGate]() {
            auto raw = engine->findSurfaceManagerShared(attackerId);
            pointerReady.release();
            if (!raw) {
                return;
            }
            callerGate.wait(); // simulated preemption; object is freed meanwhile
            // Dangling call. Classify the outcome:
            //  - direct fault (garbage vtable/unmapped): signal handler
            //    recovers -> g_feederFaults
            //  - stale bytes still holding the original object ->
            //    bad_weak_ptr -> g_weakThrows
            //  - block reused by live data -> the call RUNS and performs
            //    wild atomic ops / posts into the worker -> g_silentWild
            if (sigsetjmp(g_feederJmp, 1) == 0) {
                g_feederArmed = true;
                try {
                    raw->beginTextStream();
                    raw->receiveTextChunk(R"({"k":"v"})");
                    raw->endTextStream();
                    g_silentWild.fetch_add(1);
                } catch (...) {
                    g_weakThrows.fetch_add(1);
                }
                g_feederArmed = false;
            }
        });

        // ---- 3. Destroy the attacker: uninit runs on the worker, then the
        // last shared_ptr reference drops and the 248-byte block is freed.
        pointerReady.wait();
        engine->destroySurfaceManager(attacker);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));

        // ---- 4. Churn that leaves the freed block available (everything
        // allocated here is freed again before the victim streams).
        HeapChurn();

        // ---- 5. Victim streams: the worker allocates live structures; some
        // of them land in the freed attacker block (same size class), so the
        // dangling pointer now aliases live worker-thread memory. Extra
        // update rounds maximize fresh 256-class allocations just before the
        // race (live nodes/Impls occupying the block when the feeder fires).
        victim->beginTextStream();
        victim->receiveTextChunk(CreateSurfaceJson(victimSurface));
        victim->receiveTextChunk(UpdateComponentsJson(victimSurface, round));
        for (int extra = 0; extra < 3; ++extra) {
            victim->receiveTextChunk(
                UpdateComponentsJson(victimSurface, round * 10 + extra));
        }
        victim->endTextStream();
        // Let the worker process the chunks (and allocate) before the race.
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        // ---- 6. Release the racing call against whatever now occupies the
        // block.
        callerGate.release();
        feeder.join();

        // ---- 7. Victim keeps working: the worker traverses structures that
        // may already be corrupted. Update + teardown exercises exactly the
        // code paths of the three production crashes (updateChildren /
        // snapshot copy / ComponentManager & VirtualDOM destruction).
        victim->beginTextStream();
        victim->receiveTextChunk(UpdateComponentsJson(victimSurface, round + 1000000));
        victim->receiveTextChunk(DeleteSurfaceJson(victimSurface));
        victim->endTextStream();
        engine->destroySurfaceManager(victim);

        HeapChurn();

        if ((round & 255) == 0) {
            std::printf("stress: round %d ok\n", round);
            std::fflush(stdout);
        }
    }

    // Drain remaining worker tasks before engine shutdown.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    agenui::destroyAGenUIEngine();
    std::printf("stress: completed without crashing, feeder faults=%ld weak=%ld silentWild=%ld\n",
                g_feederFaults.load(), g_weakThrows.load(), g_silentWild.load());
    return 0;
}
