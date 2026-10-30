// Reproducer for the orphan-table id-cycle defects in VirtualDOM.
//
// Two mutually exclusive chains share the same id cycle (template K -> plain
// K_x -> K), selected by kPayload below:
//   A: template carries NO binding => AnyDataReady + NotDependent, which is the
//      only combination that reaches checkCanDisplay's descendant recursions.
//      Without the visited guard those recursions descend forever => SIGSEGV on
//      the stack guard page.
//   B: template carries a {"path":"/t"} binding => AnyDataReady + FullyReady,
//      which short-circuits the recursion but drives tryAttachReadyOrphans into
//      a re-entrant takeOrphanSnapshot erase => heap-use-after-free / double free.
//
// kPayload / kTemplateDir are hardcoded on purpose; edit them to switch chains.
// kTemplateDir must point at a directory holding templates.json (listing
// "RowBound" and "RowPlain") plus RowBound.json / RowPlain.json — see
// tests/cpp/fixtures/templates_orphan_cycle/.

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#include "agenui_engine.h"
#include "agenui_engine_entry.h"
#include "agenui_surface_manager_interface.h"
#include "agenui_logger_internal.h"

namespace {

// ---- edit these two to switch the reproduced chain ----
const char* const kPayload = "A";  // "A" = stack overflow, "B" = use-after-free
const char* const kTemplateDir =
    "{AGenUI_rootdir}/tests/cpp/fixtures/templates_orphan_cycle";
// -------------------------------------------------------

const char* const kSurfaceId = "orphan_cycle";

// Each chunk is one protocol event; the shared worker thread drains them FIFO,
// so the sleep only keeps batch windows from overlapping.
void sendEvent(agenui::ISurfaceManager* sm, const std::string& label, const std::string& payload) {
    std::printf("\n========== [%s] %s\n", label.c_str(), payload.c_str());
    std::fflush(stdout);
    sm->beginTextStream();
    sm->receiveTextChunk(payload);
    sm->endTextStream();
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
}

std::string updateComponents(const std::string& componentsArrayJson) {
    return std::string("{\"version\":\"v0.9\",\"updateComponents\":{\"surfaceId\":\"") + kSurfaceId +
           "\",\"components\":" + componentsArrayJson + "}}";
}

void runPayloadB(agenui::ISurfaceManager* sm) {
    // /t and /m resolve, so a {"path":"/t"} binding reports FullyReady.
    sendEvent(sm, "1 updateDataModel",
              std::string("{\"version\":\"v0.9\",\"updateDataModel\":{\"surfaceId\":\"") + kSurfaceId +
              "\",\"path\":\"/\",\"value\":{\"t\":\"hello\",\"m\":\"world\"}}}");

    // Plain K => Always, no children => _directOrphanSnapshots["K"].
    sendEvent(sm, "2 updateComponents K (plain)",
              updateComponents(R"([{"id":"K","component":"Text","text":"plain-K"}])"));

    // Plain K_x carrying the back edge of the cycle => _direct["K_x"], children ["K"].
    sendEvent(sm, "3 updateComponents K_x (plain, children=[K])",
              updateComponents(R"([{"id":"K_x","component":"Text","text":"plain-Kx","children":["K"]}])"));

    // Same id "K" as a template => displayRules["K"]=AnyDataReady; RowBound's own
    // root carries {"path":"/t"} so K aggregates to FullyReady and lands in
    // _dataDependent["K"]. "K" is now in BOTH orphan tables.
    sendEvent(sm, "4 updateComponents K (RowBound => AnyDataReady + FullyReady)",
              updateComponents(R"([{"id":"K","component":"RowBound"}])"));

    // Grow the tree; attaching K consumes only the _direct copy, leaving the
    // _dataDependent["K"] entry (children ["K_x"]) alive.
    sendEvent(sm, "5 updateComponents root+P (attaches K from the direct table)",
              updateComponents(R"([{"id":"root","component":"Column","children":["P"]},)"
                               R"({"id":"P","component":"Column","children":["K"]}])"));

    // TRIGGER: a second data-dependent orphan runs tryAttachReadyOrphans; K's
    // setSnapshot re-enters takeOrphanSnapshot("K"), erasing the node the outer
    // loop still holds, so the loop's own erase is a double free.
    sendEvent(sm, "6 updateComponents M (RowBound) -- TRIGGER",
              updateComponents(R"([{"id":"M","component":"RowBound"}])"));
}

void runPayloadA(agenui::ISurfaceManager* sm) {
    // K_x (plain, Always) declares children ["K"].
    sendEvent(sm, "1 updateComponents K_x (plain, children=[K])",
              updateComponents(R"([{"id":"K_x","component":"Text","children":["K"]}])"));

    // TRIGGER: RowPlain carries no binding, so K is AnyDataReady + NotDependent
    // and reaches the descendant recursion over the K <-> K_x cycle.
    sendEvent(sm, "2 updateComponents K (RowPlain, no binding) -- TRIGGER",
              updateComponents(R"([{"id":"K","component":"RowPlain"}])"));
}

}  // namespace

int main() {
    agenui::setDefaultLogMinLevel(agenui::LOG_LEVEL_DEBUG);

    agenui::IAGenUIEngine* engine = agenui::initAGenUIEngine();
    if (!engine) {
        std::fprintf(stderr, "initAGenUIEngine failed\n");
        return 2;
    }

    if (!engine->setPathConfig(std::string("{\"templateDir\":\"") + kTemplateDir + "\"}")) {
        std::fprintf(stderr, "setPathConfig failed for templateDir=%s\n", kTemplateDir);
    }

    agenui::ISurfaceManager* sm = engine->createSurfaceManager();
    if (!sm) {
        std::fprintf(stderr, "createSurfaceManager failed\n");
        agenui::destroyAGenUIEngine();
        return 2;
    }

    sm->beginTextStream();
    sm->endTextStream();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    sendEvent(sm, "0 createSurface",
              std::string("{\"version\":\"v0.9\",\"createSurface\":{\"surfaceId\":\"") + kSurfaceId +
              "\",\"catalogId\":\"test\",\"theme\":{},\"sendDataModel\":false,\"animated\":true}}");

    if (std::string(kPayload) == "A") {
        runPayloadA(sm);
    } else {
        runPayloadB(sm);
    }

    std::printf("\n========== teardown (destroySurfaceManager)\n");
    std::fflush(stdout);
    engine->destroySurfaceManager(sm);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    agenui::destroyAGenUIEngine();

    std::printf("\nrepro_orphan_iterator_uaf: NO CRASH (payload %s)\n", kPayload);
    return 0;
}
