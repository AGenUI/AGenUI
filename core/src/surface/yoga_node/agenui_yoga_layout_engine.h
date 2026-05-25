#pragma once

/**
 * @file agenui_yoga_layout_engine.h
 * @brief Concrete ILayoutDelegate that drives Yoga.
 *
 * Hard constraint #1 (zero business-type leak): every public method of
 * YogaLayoutEngine accepts only opaque ids, primitive numbers, or
 * @ref ILayoutDataWrapper. ComponentSnapshot does not appear anywhere in
 * this header.
 *
 * Hard constraint #2 (decoder + wrapper are mandatory): the engine's
 * onSnapshotChanged routes every style/attribute through the decoder
 * family (subclasses of @ref YogaPropertyDecoder), which read exclusively
 * from the wrapper.
 */

#include "surface/yoga_node/agenui_layout_delegate.h"

#include <map>
#include <memory>
#include <string>

namespace agenui {

class YogaNodeManager;
class YogaNode;
class IMeasurementManager;
class YogaPropertyDecoder;
class VirtualDOMNode;

/**
 * @brief Default Yoga-backed implementation of ILayoutDelegate.
 *
 * Owns a YogaNodeManager internally; that internal type stays a Yoga
 * implementation detail and is never exposed.
 */
class YogaLayoutEngine final : public ILayoutDelegate {
public:
    explicit YogaLayoutEngine(IMeasurementManager* measurementManager = nullptr);
    ~YogaLayoutEngine() override;

    YogaLayoutEngine(const YogaLayoutEngine&) = delete;
    YogaLayoutEngine& operator=(const YogaLayoutEngine&) = delete;

    // ---------------- ILayoutDelegate ----------------

    YogaNode* createNode(const std::string& nodeId) override;
    void removeNode(const std::string& nodeId) override;
    void insertChild(const std::string& parentId,
                     const std::string& childId,
                     uint32_t index) override;
    void removeChild(const std::string& parentId,
                     const std::string& childId) override;
    void clearAll() override;
    void setRootNode(YogaNode* node) override;

    void onSnapshotChanged(const std::string& nodeId,
                           ILayoutDataWrapper& wrapper,
                           const ILayoutDataWrapper* parentWrapper) override;

    void setPlatformSize(const std::string& nodeId,
                         ILayoutDataWrapper& wrapper,
                         float width, float height) override;

    void setupMeasureFunctionIfNeeded(const std::string& nodeId,
                                      ILayoutDataWrapper& wrapper) override;

    bool calculateLayout(const std::string& rootId,
                         float surfaceWidth) override;

    void updateTabsSelectedIndex(const std::string& tabsId,
                                 int selectedIndex) override;

    bool readLayoutResult(const std::string& nodeId,
                          float& outX, float& outY,
                          float& outWidth, float& outHeight) override;

    bool hasNewLayout(const std::string& nodeId) const override;
    void clearNewLayout(const std::string& nodeId) override;

    void setSnapshotChangedCallback(LayoutSnapshotChangedCallback cb) override;

    void setPropertyDecoder(std::shared_ptr<YogaPropertyDecoder> decoder) override;

    /**
     * @brief Calculate layout with Tabs height adjustment (legacy VirtualDOM integration).
     *
     * @param root VirtualDOM root node (for recursively traversing all Tabs nodes)
     * @param surfaceWidth Surface width
     * @return true if a secondary layout was performed
     */
    bool calculateLayoutWithAdjust(const std::shared_ptr<VirtualDOMNode>& root, float surfaceWidth);

private:
    std::unique_ptr<YogaNodeManager>     _manager;
    IMeasurementManager*                 _measurementManager = nullptr;
    LayoutSnapshotChangedCallback        _snapshotChangedCb;
    std::shared_ptr<YogaPropertyDecoder> _propertyDecoder;
};

}  // namespace agenui
