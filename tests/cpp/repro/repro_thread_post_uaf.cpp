// Reproducer: IThread* used after the MessageThread was destroyed
// (deterministic under ASan). Faults on the calling thread.
//
// Scenario:
//   Every SurfaceManager public API runs, on the CALLING thread:
//       IThread* messageThread = getMessageThread();   // raw IThread*
//       messageThread->post(...);
//   getMessageThread() fetches the raw pointer from ThreadManager under a
//   mutex but the pointer is USED OUTSIDE the mutex. Meanwhile
//   AGenUIEngine::stop() (via destroyAGenUIEngine) calls
//   ThreadManager::destroyThread(), which erases the entry, stop()s and
//   SAFELY_DELETEs the MessageThread object. A calling thread that already
//   obtained the raw pointer then calls post() on freed memory: lock of a
//   destroyed mutex, push into a destroyed queue, notify on a destroyed
//   condition variable — wild writes into memory reused by the allocator.
//
// This program forces the window deterministically:
//   T1: raw = ThreadManager::getMessageThread(AGENUI_SHARED_THREAD_ID)
//       then BLOCK on a gate           <-- simulates being preempted
//   main: destroyAGenUIEngine()        -> stop() -> destroyThread()
//           -> MessageThread deleted
//         release T1
//   T1: raw->post([]{}) on the freed MessageThread
//
// Expected result on UNFIXED code built with ASan:
//   ERROR: AddressSanitizer: heap-use-after-free (process aborts)
// After the fix (getMessageThread returns shared_ptr<IThread> / destroy
// defers deletion to the last reference): prints PASS and exits 0.

#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

#include "agenui_engine_entry.h"
#include "agenui_type_define.h"
#include "module/agenui_ithread.h"
#include "module/agenui_thread_manager.h"

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

Gate g_callerGate;
Gate g_pointerReady;

}  // namespace

int main() {
    agenui::IAGenUIEngine* engine = agenui::initAGenUIEngine();
    if (!engine) {
        std::fprintf(stderr, "repro_thread_post_uaf: init failed\n");
        return 2;
    }

    std::thread caller([]() {
        // Exactly what every SurfaceManager API does on the calling thread
        // (post-fix: shared ownership, so the MessageThread cannot be
        // deleted while this call is pending):
        auto raw =
            agenui::ThreadManager::getInstance().getMessageThread(AGENUI_SHARED_THREAD_ID);
        if (!raw) {
            std::fprintf(stderr, "repro_thread_post_uaf: getMessageThread null\n");
            g_pointerReady.release();
            return;
        }
        g_pointerReady.release(); // pointer acquired, now "preempted"
        g_callerGate.wait();      // <-- preemption window (deterministic)

        // Resume: on unfixed code `raw` is a deleted MessageThread.
        // post() locks its destroyed _queueMutex and pushes into the
        // destroyed _taskQueue => heap-use-after-free under ASan.
        raw->post([] {});
        std::printf("repro_thread_post_uaf: PASS (thread object stayed alive)\n");
    });

    g_pointerReady.wait();

    // engine->stop() runs destroyThread(AGENUI_SHARED_THREAD_ID):
    // erase from map -> stop() -> SAFELY_DELETE(thread).
    agenui::destroyAGenUIEngine();

    g_callerGate.release(); // T1 now posts on the freed thread object
    caller.join();
    return 0;
}
