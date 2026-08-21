// Reproducer: ISurfaceManager* used after the object was destroyed
// (deterministic under ASan). Faults on the calling thread.
//
// Scenario:
//   JNI layer hands Java threads a RAW ISurfaceManager* obtained from
//   AGenUIEngine::findSurfaceManager() (it->second.get()). A raw pointer
//   does not participate in shared_ptr reference counting, so the object
//   can be released (on the worker thread, after the posted uninit lambda
//   finishes) while the Java thread is still about to invoke a public API
//   on it. The first member accesses inside the API body
//   (_instanceId read in the perf log, _isRunning.load(),
//   shared_from_this()) then become use-after-free.
//
// This program reproduces the "preemption window" deterministically:
//   T1: fetch raw ISurfaceManager* from the engine map (as JNI does),
//       then BLOCK on a semaphore  <-- simulates being preempted
//   main: destroySurfaceManager()  -> uninit lambda runs on the worker
//           -> lambda ends -> last shared_ptr reference drops
//           -> SurfaceManager is freed on the worker thread
//         then release T1
//   T1: resumes and calls receiveTextChunk() on the freed object
//
// Expected result on the UNFIXED code, built with ASan:
//   ERROR: AddressSanitizer: heap-use-after-free  (process aborts)
// After the fix (JNI/engine APIs hold a shared_ptr across the call):
//   prints "repro_sm_uaf: PASS (object stayed alive)" and exits 0.

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>

#include "agenui_engine.h"
#include "agenui_engine_entry.h"
#include "agenui_surface_manager_interface.h"

namespace {

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

Gate g_callerGate;   // main -> T1: "object is freed, resume and call"
Gate g_pointerReady; // T1 -> main: "I hold the raw pointer"

}  // namespace

int main() {
    agenui::IAGenUIEngine* engine = agenui::initAGenUIEngine();
    if (!engine) {
        std::fprintf(stderr, "repro_sm_uaf: initAGenUIEngine failed\n");
        return 2;
    }

    agenui::ISurfaceManager* sm = engine->createSurfaceManager();
    if (!sm) {
        std::fprintf(stderr, "repro_sm_uaf: createSurfaceManager failed\n");
        return 2;
    }
    const int instanceId = sm->getInstanceId();

    // T1 plays the role of a Java thread inside a JNI entry function:
    // it already resolved the handle and is about to call the API.
    std::thread caller([engine, instanceId]() {
        // Exactly what jni_receiveTextChunk does first (post-fix: shared
        // ownership, so the object cannot be freed while this call runs):
        auto raw = engine->findSurfaceManagerShared(instanceId);
        if (!raw) {
            std::fprintf(stderr, "repro_sm_uaf: find failed\n");
            g_pointerReady.release();
            return;
        }
        g_pointerReady.release(); // "pointer acquired, now 'preempted'"
        g_callerGate.wait();      // <-- preemption window (deterministic)

        // Resume: on unfixed code `raw` is dangling here. On ASan the very
        // first member access inside receiveTextChunk is reported as
        // heap-use-after-free and the process aborts.
        raw->receiveTextChunk(R"({"k":"v"})");
        std::printf("repro_sm_uaf: PASS (object stayed alive during call)\n");
    });

    g_pointerReady.wait();

    // Teardown path: destroySurfaceManager posts uninit; the object is
    // released when the uninit lambda (last shared_ptr owner) finishes.
    engine->destroySurfaceManager(sm);

    // Drain the worker FIFO with a throwaway SurfaceManager: the sync
    // session below can only execute after the pending uninit lambda, so
    // by the time its listener callback fires, the old SurfaceManager is
    // guaranteed to be freed.
    agenui::ISurfaceManager* syncSm = engine->createSurfaceManager();
    if (syncSm) {
        syncSm->beginTextStream();
        syncSm->receiveTextChunk(
            R"({"version":"v0.9","createSurface":{"surfaceId":"_sync_","catalogId":"sync","theme":{},"sendDataModel":false,"animated":true}})");
        syncSm->endTextStream();
        // Give the worker time to process the sync session and free the
        // destroyed SurfaceManager. (A listener could be used instead; the
        // sleep keeps this reproducer dependency-free.)
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        engine->destroySurfaceManager(syncSm);
    }

    g_callerGate.release(); // T1 now calls the API on the freed object
    caller.join();

    agenui::destroyAGenUIEngine();
    return 0;
}
