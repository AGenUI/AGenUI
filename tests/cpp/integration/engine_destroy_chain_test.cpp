// DC*: Engine destroy chain death tests.
//
// Architectural constraint:
//   AGenUIEngine is a process-level singleton initialised with std::call_once.
//   After destroyAGenUIEngine() the engine cannot be re-created in the same
//   process. The global test Environment (support/test_env.h) owns the
//   lifecycle, so individual tests must NOT call destroy — unless they run in
//   a subprocess.
//
//   GTest death tests (EXPECT_EXIT with "threadsafe" style) solve this by
//   re-executing the test binary in a child process. The child goes through
//   main() → Environment::SetUp() (initAGenUIEngine) → death test body →
//   _exit(0). The parent verifies exit code 0 = no crash / no sanitizer
//   abort / no deadlock.
//
// Exercises:
//   initAGenUIEngine  (via global Environment in the subprocess)
//   createSurfaceManager
//   beginTextStream
//   receiveTextChunk
//   endTextStream
//   destroySurfaceManager
//   destroyAGenUIEngine

#include <gtest/gtest.h>

#include <string>

#include "agenui_engine.h"
#include "agenui_engine_entry.h"
#include "agenui_surface_manager_interface.h"
#include "support/mock_message_listener.h"
#include "support/test_env.h"
#include "support/thread_sync_helper.h"

namespace {

// Minimal valid A2UI protocol JSON — inlined to avoid fixture file I/O in
// the re-exec'd subprocess where CWD may differ.

static std::string createSurfaceJson(const std::string& surfaceId) {
    return R"({"version":"v0.9","createSurface":{"surfaceId":")" + surfaceId +
           R"(","catalogId":"https://a2ui.org/specification/v0_9/standard_catalog.json","theme":{},"sendDataModel":false,"animated":true}})";
}

static std::string updateComponentsJson(const std::string& surfaceId) {
    return R"({"version":"v0.9","updateComponents":{"surfaceId":")" +
           surfaceId +
           R"(","components":[{"id":"text_1","component":"Text","text":"hello"}]}})";
}

// ---------------------------------------------------------------------------
// Death test body helpers.
//
// Extracted as standalone functions so that EXPECT_EXIT's macro expansion
// never sees bare commas (e.g. variable declarations like "l1, l2, l3")
// inside its first argument — the C preprocessor would miscount macro args.
// ---------------------------------------------------------------------------

// DC001 body: full lifecycle.
static void runDC001() {
    auto* engine = ::agenui::getAGenUIEngine();
    if (!engine) _exit(1);

    ::agenui::testing::MockMessageListener listener;

    auto* sm = engine->createSurfaceManager();
    if (!sm) _exit(2);
    // Must wait: init() runs on worker thread; without it the SM's
    // internal modules (_dispatcher, _streamingContentParser) aren't
    // created yet and streaming calls would hit null pointers.
    ::agenui::testing::WaitForWorkerIdle();

    sm->addSurfaceEventListener(&listener);

    // Session 1: createSurface
    sm->beginTextStream();
    sm->receiveTextChunk(createSurfaceJson("dc001"));
    sm->endTextStream();
    // Wait between sessions so session 1 completes before session 2
    // begins (beginTextStream resets parser state).
    ::agenui::testing::WaitForWorkerIdle();

    // Session 2: updateComponents
    sm->beginTextStream();
    sm->receiveTextChunk(updateComponentsJson("dc001"));
    sm->endTextStream();
    // No drain before destroy — engine->stop() drains the worker thread
    // itself via ThreadManager::destroyThread(), which joins the thread
    // and processes all pending tasks before any object is deleted.

    sm->removeSurfaceEventListener(&listener);
    engine->destroySurfaceManager(sm);

    // Destroy engine directly — stop() will drain pending uninit() tasks.
    ::agenui::destroyAGenUIEngine();

    _exit(0);
}

// DC002 body: mid-stream destroy.
static void runDC002() {
    auto* engine = ::agenui::getAGenUIEngine();
    if (!engine) _exit(1);

    ::agenui::testing::MockMessageListener listener;

    auto* sm = engine->createSurfaceManager();
    if (!sm) _exit(2);
    ::agenui::testing::WaitForWorkerIdle();

    sm->addSurfaceEventListener(&listener);

    // Start a streaming session but do NOT end it
    sm->beginTextStream();
    sm->receiveTextChunk(createSurfaceJson("dc002"));
    sm->receiveTextChunk(R"({"version":"v0.9","updateCompon)");
    // Deliberately leave session open — no endTextStream()

    // Do NOT remove listener — tests uninit() robustness
    engine->destroySurfaceManager(sm);

    // No drain — engine->stop() drains the worker thread itself.
    ::agenui::destroyAGenUIEngine();

    _exit(0);
}

// DC003 body: multiple SurfaceManagers.
static void runDC003() {
    auto* engine = ::agenui::getAGenUIEngine();
    if (!engine) _exit(1);

    ::agenui::testing::MockMessageListener l1;
    ::agenui::testing::MockMessageListener l2;
    ::agenui::testing::MockMessageListener l3;

    auto* sm1 = engine->createSurfaceManager();
    auto* sm2 = engine->createSurfaceManager();
    auto* sm3 = engine->createSurfaceManager();
    if (!sm1 || !sm2 || !sm3) _exit(2);
    ::agenui::testing::WaitForWorkerIdle();

    sm1->addSurfaceEventListener(&l1);
    sm2->addSurfaceEventListener(&l2);
    sm3->addSurfaceEventListener(&l3);

    // sm1: complete session
    sm1->beginTextStream();
    sm1->receiveTextChunk(createSurfaceJson("dc003-s1"));
    sm1->endTextStream();
    ::agenui::testing::WaitForWorkerIdle();

    // sm2: mid-stream (no endTextStream)
    sm2->beginTextStream();
    sm2->receiveTextChunk(createSurfaceJson("dc003-s2"));

    // sm3: complete first session, start second mid-stream
    sm3->beginTextStream();
    sm3->receiveTextChunk(createSurfaceJson("dc003-s3"));
    sm3->endTextStream();
    ::agenui::testing::WaitForWorkerIdle();

    sm3->beginTextStream();
    sm3->receiveTextChunk(updateComponentsJson("dc003-s3"));
    // second session left open

    // Destroy all SMs — listeners still attached on sm2/sm3
    sm1->removeSurfaceEventListener(&l1);
    engine->destroySurfaceManager(sm1);
    engine->destroySurfaceManager(sm2);
    engine->destroySurfaceManager(sm3);

    // No drain — engine->stop() drains the worker thread itself.
    ::agenui::destroyAGenUIEngine();

    _exit(0);
}

// DC004 body: rapid create/destroy cycles.
static void runDC004() {
    auto* engine = ::agenui::getAGenUIEngine();
    if (!engine) _exit(1);

    // Rapid cycles — intentionally skip WaitForWorkerIdle to let
    // worker-thread tasks pile up. No listener in the loop because the
    // SM is destroyed without draining; a stack-local listener would be
    // use-after-scope by the time the worker thread dispatches callbacks.
    for (int i = 0; i < 10; ++i) {
        auto* sm = engine->createSurfaceManager();
        if (!sm) _exit(2);

        sm->beginTextStream();
        sm->receiveTextChunk(
            createSurfaceJson("dc004-" + std::to_string(i)));
        sm->endTextStream();

        engine->destroySurfaceManager(sm);
        // No drain — tasks accumulate on worker thread
    }

    // Drain all accumulated tasks
    ::agenui::testing::WaitForWorkerIdle(5000);

    // Final SM — proves engine is still fully operational after churn
    auto* smFinal = engine->createSurfaceManager();
    if (!smFinal) _exit(3);
    ::agenui::testing::WaitForWorkerIdle();

    ::agenui::testing::MockMessageListener listenerFinal;
    smFinal->addSurfaceEventListener(&listenerFinal);

    smFinal->beginTextStream();
    smFinal->receiveTextChunk(createSurfaceJson("dc004-final"));
    smFinal->endTextStream();
    ::agenui::testing::WaitForWorkerIdle();

    smFinal->removeSurfaceEventListener(&listenerFinal);
    engine->destroySurfaceManager(smFinal);

    // No drain — engine->stop() drains the worker thread itself.
    ::agenui::destroyAGenUIEngine();

    _exit(0);
}

// ---------------------------------------------------------------------------
// Test suite — "DeathTest" suffix makes GTest default to threadsafe style.
// ---------------------------------------------------------------------------
class EngineDestroyChainDeathTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ::testing::GTEST_FLAG(death_test_style) = "threadsafe";
    }
};

// DC001: Full lifecycle destroy chain.
//
// Covers all 6 APIs in a single subprocess run:
//   initAGenUIEngine (global env) → createSurfaceManager →
//   beginTextStream → receiveTextChunk → endTextStream →
//   destroySurfaceManager → destroyAGenUIEngine
//
// Two complete streaming sessions are exercised (createSurface +
// updateComponents) to prove the engine processes real protocol data
// before being torn down.
TEST_F(EngineDestroyChainDeathTest, DC001_FullLifecycleDestroyChain) {
    EXPECT_EXIT(runDC001(), ::testing::ExitedWithCode(0), "");
}

// DC002: Mid-stream destroy (incomplete streaming session).
//
// beginTextStream + receiveTextChunk without endTextStream, then tear down
// SM and engine. Verifies the streaming parser handles partial/buffered
// data during uninit(), and that a still-attached listener doesn't cause
// use-after-free (uninit removes all listeners first).
TEST_F(EngineDestroyChainDeathTest, DC002_MidStreamDestroy) {
    EXPECT_EXIT(runDC002(), ::testing::ExitedWithCode(0), "");
}

// DC003: Multiple SurfaceManagers in different states.
//
// Creates 3 SMs:
//   sm1 — completed session (begin → chunk → end)
//   sm2 — mid-stream (begin → chunk, no end)
//   sm3 — completed session + second session mid-stream
// All three are destroyed explicitly, then engine is torn down.
TEST_F(EngineDestroyChainDeathTest, DC003_MultipleSurfaceManagersDestroy) {
    EXPECT_EXIT(runDC003(), ::testing::ExitedWithCode(0), "");
}

// DC004: Rapid create/destroy cycles before engine destroy.
//
// 10 rapid cycles of create → stream → destroy (without draining between
// cycles), followed by a final SM to prove the engine is still functional.
// Stresses the shared_ptr reference-counting mechanism that keeps SM alive
// while worker-thread lambdas still reference it.
TEST_F(EngineDestroyChainDeathTest, DC004_RapidCreateDestroyCycles) {
    EXPECT_EXIT(runDC004(), ::testing::ExitedWithCode(0), "");
}

}  // namespace
