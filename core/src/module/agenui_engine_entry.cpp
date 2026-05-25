#include "agenui_engine_entry.h"
#include <atomic>
#include <mutex>
#include "agenui_engine_impl.h"
#include "agenui_logger_internal.h"
#include "agenui_type_define.h"

namespace agenui {

// Global engine instance, protected by std::call_once to ensure thread-safe initialization
static std::once_flag g_initFlag;
static std::atomic<AGenUIEngine*> g_agenUIEngine(nullptr);

IAGenUIEngine* initAGenUIEngine() {
    std::call_once(g_initFlag, []() {
        auto* engine = new AGenUIEngine();
        engine->start();
        g_agenUIEngine = engine;
        AGENUI_LOG("engine:%p", engine);
    });
    return g_agenUIEngine;
}

IAGenUIEngine* getAGenUIEngine() {
    return g_agenUIEngine;
}

void destroyAGenUIEngine() {
    auto* engine = g_agenUIEngine.exchange(nullptr);
    AGENUI_LOG("engine:%p", engine);
    if (engine) {
        engine->stop();
        SAFELY_DELETE(engine);
    }
}

} // namespace agenui
