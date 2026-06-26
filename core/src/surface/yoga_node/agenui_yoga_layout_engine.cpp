#include "surface/yoga_node/agenui_yoga_layout_engine.h"

#include "surface/yoga_node/agenui_component_snapshot_wrapper.h"
#include "surface/yoga_node/agenui_yoga_node.h"
#include "surface/yoga_node/agenui_yoga_node_manager.h"
#include "surface/yoga_node/agenui_yoga_property_decoder.h"
#include "surface/yoga_node/agenui_measurement_manager.h"
#include "surface/yoga_node/agenui_tabs_yoga_helper.h"
#include "surface/virtual_dom/agenui_virtual_dom_node.h"
#include "agenui_logger_internal.h"

#include <yoga/Yoga.h>

namespace agenui {

YogaLayoutEngine::YogaLayoutEngine(IMeasurementManager* measurementManager)
    : _manager(std::make_unique<YogaNodeManager>())
    , _measurementManager(measurementManager)
    , _propertyDecoder(std::make_shared<BuiltinYogaPropertyDecoder>()) {}

YogaLayoutEngine::~YogaLayoutEngine() = default;

// ---------------- node lifecycle ----------------

YogaNode* YogaLayoutEngine::createNode(const std::string& nodeId) {
    return _manager->createNode(nodeId);
}

void YogaLayoutEngine::removeNode(const std::string& nodeId) {
    _manager->removeNode(nodeId);
}

void YogaLayoutEngine::insertChild(const std::string& parentId,
                                   const std::string& childId,
                                   uint32_t index) {
    _manager->insertChild(parentId, childId, index);
}

void YogaLayoutEngine::removeChild(const std::string& parentId,
                                   const std::string& childId) {
    _manager->removeChild(parentId, childId);
}

void YogaLayoutEngine::clearAll() {
    _manager->clearAll();
}

void YogaLayoutEngine::setRootNode(YogaNode* node) {
    _manager->setRootNode(node);
}

// ---------------- snapshot driven update ----------------

void YogaLayoutEngine::onSnapshotChanged(const std::string& nodeId,
                                        ILayoutDataWrapper& wrapper,
                                        const ILayoutDataWrapper* parentWrapper) {
    YogaNode* node = _manager->getNode(nodeId);
    if (!node) return;

    // Route property decoding through the injected decoder (default is
    // BuiltinYogaPropertyDecoder). SDK consumers swap in their own via
    // setPropertyDecoder().
    if (_propertyDecoder) {
        _propertyDecoder->apply(wrapper, node->get(), /*clearAfterDecode=*/true);
        node->saveCurrentStyleSize();
    }

    // Tabs-rule: if the PARENT is a Tabs component, this child needs an
    // absolute-position layout hint. We only rely on the public ILayoutDataWrapper
    // interface (componentType()) here, no business-type leak.
    if (parentWrapper && parentWrapper->componentType() == "Tabs") {
        TabsYogaHelper::setChildAbsoluteLayout(*node, nodeId);
    }
}

void YogaLayoutEngine::setPlatformSize(const std::string& nodeId,
                                       ILayoutDataWrapper& wrapper,
                                       float width, float height) {
    YogaNode* node = _manager->getNode(nodeId);
    if (!node) return;
    node->clearMeasureFunc();
    node->setSize(width, height);
    node->markDirty();
    wrapper.setPlatformSizeLocked(true);
}

// ---------------- measurement ----------------

void YogaLayoutEngine::setupMeasureFunctionIfNeeded(const std::string& nodeId,
                                                    ILayoutDataWrapper& wrapper) {
    if (!_measurementManager) return;

    YogaNode* node = _manager->getNode(nodeId);
    if (!node) return;

    // shouldUseMeasureFunc / MeasureDecisionContext / MeasureDecision are
    // INTERNAL types (declared in agenui_measurement_manager.h), not part of
    // the public IMeasurementManager interface. Engine pairs with the
    // bundled MeasurementManagerImpl and does the downcast here (Phase A
    // Hard constraint 2 — the cast lives in the concrete consumer).
    auto* mgrImpl = static_cast<MeasurementManagerImpl*>(_measurementManager);
    ::agenui::MeasureDecision decision = ::agenui::MeasureDecision::Skip;
    if (mgrImpl) {
        MeasureDecisionContext ctx;
        ctx.hasChildren        = node->childCount() > 0;
        ctx.platformSizeLocked = wrapper.platformSizeLocked();
        ctx.widthStyle         = wrapper.styleAsString("width");
        ctx.heightStyle        = wrapper.styleAsString("height");
        ctx.aspectRatioStyle   = wrapper.styleAsString("aspect-ratio");
        decision = mgrImpl->shouldUseMeasureFunc(wrapper.componentType(), ctx);
    } else if (_measurementManager->getMeasurement(wrapper.componentType())) {
        decision = wrapper.platformSizeLocked()
                       ? ::agenui::MeasureDecision::Clear
                       : ::agenui::MeasureDecision::Register;
    }

    switch (decision) {
        case ::agenui::MeasureDecision::Clear:
            node->clearMeasureFunc();
            break;

        case ::agenui::MeasureDecision::Register: {
            auto* mgr = _measurementManager;
            std::string type = wrapper.componentType();
            // weak_ptr instead of raw pointer: if the wrapper is destroyed
            // before Yoga schedules this callback on the layout thread, lock()
            // returns null and we fall back to YGUndefined sizing instead of UAF.
            std::weak_ptr<ILayoutDataWrapper> wrapperWeak = wrapper.weak_from_this();
            node->setMeasureFunc(
                [mgr, type, wrapperWeak](float w, YGMeasureMode wMode,
                                         float h, YGMeasureMode hMode) -> YGSize {
                    if (!mgr) return YGSize{YGUndefined, YGUndefined};
                    auto wrapperLocked = wrapperWeak.lock();
                    if (!wrapperLocked) return YGSize{YGUndefined, YGUndefined};
                    MeasureModes modes;
                    modes.width.maxValue  = w;
                    modes.width.mode      = static_cast<int>(wMode);
                    modes.height.maxValue = h;
                    modes.height.mode     = static_cast<int>(hMode);
                    auto result = mgr->measure(type,
                                               wrapperLocked->serializeForMeasure(),
                                               modes);
                    return YGSize{result.width, result.height};
                });
            break;
        }

        case ::agenui::MeasureDecision::Skip:
        default:
            break;
    }
}

// ---------------- layout calculation ----------------

bool YogaLayoutEngine::calculateLayout(const std::string& rootId,
                                       float surfaceWidth) {
    YogaNode* root = _manager->getNode(rootId);
    if (!root) return false;
    _manager->calculateLayout(surfaceWidth);
    return true;
}

void YogaLayoutEngine::updateTabsSelectedIndex(const std::string& tabsId,
                                               int selectedIndex) {
    _manager->updateTabsSelectedIndex(tabsId, selectedIndex);
}

// ---------------- layout result query ----------------

bool YogaLayoutEngine::readLayoutResult(const std::string& nodeId,
                                        float& outX, float& outY,
                                        float& outWidth, float& outHeight) {
    YogaNode* node = _manager->getNode(nodeId);
    if (!node) {
        outX = outY = outWidth = outHeight = 0.0f;
        return false;
    }
    outX      = node->getLayoutLeft();
    outY      = node->getLayoutTop();
    outWidth  = node->getLayoutWidth();
    outHeight = node->getLayoutHeight();
    return true;
}

bool YogaLayoutEngine::hasNewLayout(const std::string& nodeId) const {
    YogaNode* node = _manager->getNode(nodeId);
    return node && node->hasNewLayout();
}

void YogaLayoutEngine::clearNewLayout(const std::string& nodeId) {
    YogaNode* node = _manager->getNode(nodeId);
    if (node) node->clearNewLayout();
}

// ---------------- observer ----------------

void YogaLayoutEngine::setSnapshotChangedCallback(LayoutSnapshotChangedCallback cb) {
    _snapshotChangedCb = std::move(cb);
}

void YogaLayoutEngine::setPropertyDecoder(std::shared_ptr<YogaPropertyDecoder> decoder) {
    // nullptr restores the bundled default so onSnapshotChanged never
    // silently no-ops on the decoder branch.
    _propertyDecoder = decoder ? std::move(decoder)
                               : std::make_shared<BuiltinYogaPropertyDecoder>();
}

// ---------------- legacy VirtualDOM integration ----------------

bool YogaLayoutEngine::calculateLayoutWithAdjust(const std::shared_ptr<VirtualDOMNode>& root,
                                                 float surfaceWidth) {
    if (!root) return false;
    return _manager->calculateLayoutWithAdjust(root, surfaceWidth);
}

}  // namespace agenui
