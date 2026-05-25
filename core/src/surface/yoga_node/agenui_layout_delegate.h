#pragma once

/**
 * @file agenui_layout_delegate.h
 * @brief Public SDK contract for plugging a layout engine.
 *
 * Implement this interface to swap the bundled Yoga-backed engine for
 * your own (e.g. Skia, Taffy, custom CAPI). All AGenUI surfaces consume
 * the engine exclusively via this interface.
 *
 * Hard constraint #1 (zero business-type leak): no method in this interface
 * accepts or returns ComponentSnapshot or any other business type. The
 * delegate sees only:
 *   - opaque node ids (std::string)
 *   - ILayoutDataWrapper (which does the type erasure)
 *   - primitive layout numbers
 *
 * The implementation (YogaLayoutEngine) owns all Yoga / TabsHelper
 * specialisations internally.
 *
 * @par SDK consumers
 *   Header location is intentionally kept under `surface/yoga_node/` so
 *   the existing repo layout is preserved. SDK consumers add `core/src`
 *   to HEADER_SEARCH_PATHS (already done by the bundled AGenUI.podspec)
 *   and include via:
 *       \#include "surface/yoga_node/agenui_layout_delegate.h"
 *   See `core/src/surface/yoga_node/README.md` for the extension model.
 */

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace agenui {

class ILayoutDataWrapper;
class YogaPropertyDecoder;
class YogaNode;

/**
 * @brief Notification callback when a node's layout snapshot has changed.
 *
 * Carries an ILayoutDataWrapper rather than a ComponentSnapshot so that the
 * delegate stays free of business types.
 */
using LayoutSnapshotChangedCallback =
    std::function<void(const std::string& nodeId,
                       const ILayoutDataWrapper& wrapper)>;

/**
 * @brief Layout delegate.
 *
 * Replaces direct YogaNode / YogaNodeManager access from VirtualDOM /
 * VirtualDOMNode. The delegate is the single facade through which the DOM
 * layer drives layout.
 *
 * Lifecycle: owned by VirtualDOM. Created once per VirtualDOM instance and
 * released together with it.
 */
class ILayoutDelegate {
public:
    virtual ~ILayoutDelegate() = default;

    // ---------------- node lifecycle ----------------

    /**
     * @brief Create a layout node.
     * @param nodeId Unique identifier for the node
     * @return Pointer to the created YogaNode, or nullptr on failure
     */
    virtual YogaNode* createNode(const std::string& nodeId) = 0;
    virtual void removeNode(const std::string& nodeId) = 0;
    virtual void insertChild(const std::string& parentId,
                             const std::string& childId,
                             uint32_t index) = 0;
    virtual void removeChild(const std::string& parentId,
                             const std::string& childId) = 0;
    virtual void clearAll() = 0;

    /**
     * @brief Set the root node for layout calculations.
     * @param node Pointer to the root YogaNode
     */
    virtual void setRootNode(YogaNode* node) = 0;

    // ---------------- snapshot driven update ----------------

    virtual void onSnapshotChanged(const std::string& nodeId,
                                   ILayoutDataWrapper& wrapper,
                                   const ILayoutDataWrapper* parentWrapper) = 0;

    virtual void setPlatformSize(const std::string& nodeId,
                                 ILayoutDataWrapper& wrapper,
                                 float width, float height) = 0;

    // ---------------- measurement registration ----------------

    virtual void setupMeasureFunctionIfNeeded(const std::string& nodeId,
                                              ILayoutDataWrapper& wrapper) = 0;

    // ---------------- layout calculation ----------------

    virtual bool calculateLayout(const std::string& rootId,
                                 float surfaceWidth) = 0;

    virtual void updateTabsSelectedIndex(const std::string& tabsId,
                                         int selectedIndex) = 0;

    // ---------------- layout result query ----------------

    virtual bool readLayoutResult(const std::string& nodeId,
                                  float& outX, float& outY,
                                  float& outWidth, float& outHeight) = 0;

    virtual bool hasNewLayout(const std::string& nodeId) const = 0;
    virtual void clearNewLayout(const std::string& nodeId) = 0;

    // ---------------- observer ----------------

    virtual void setSnapshotChangedCallback(LayoutSnapshotChangedCallback cb) = 0;

    // ---------------- decoder injection ----------------

    /**
     * @brief Inject a custom YogaPropertyDecoder used by onSnapshotChanged().
     *
     * The engine ships with a built-in default decoder (BuiltinYogaPropertyDecoder).
     * Pass nullptr to restore the default. Pass your own shared_ptr to plug
     * a custom decoder paired with your wrapper subclass.
     *
     * Threading: must be called before the first onSnapshotChanged on the
     * same delegate instance.
     */
    virtual void setPropertyDecoder(std::shared_ptr<YogaPropertyDecoder> decoder) = 0;
};

/**
 * @brief No-op layout delegate.
 *
 * Used in headless / offline scenarios where a real engine is not needed.
 * Every operation is a successful no-op; readLayoutResult returns false.
 */
class NullLayoutDelegate final : public ILayoutDelegate {
public:
    YogaNode* createNode(const std::string&) override { return nullptr; }
    void removeNode(const std::string&) override {}
    void insertChild(const std::string&, const std::string&, uint32_t) override {}
    void removeChild(const std::string&, const std::string&) override {}
    void clearAll() override {}
    void setRootNode(YogaNode*) override {}
    void onSnapshotChanged(const std::string&,
                           ILayoutDataWrapper&,
                           const ILayoutDataWrapper*) override {}
    void setPlatformSize(const std::string&,
                         ILayoutDataWrapper&,
                         float, float) override {}
    void setupMeasureFunctionIfNeeded(const std::string&,
                                      ILayoutDataWrapper&) override {}
    bool calculateLayout(const std::string&, float) override { return false; }
    void updateTabsSelectedIndex(const std::string&, int) override {}
    bool readLayoutResult(const std::string&,
                          float& x, float& y,
                          float& w, float& h) override {
        x = y = w = h = 0.0f;
        return false;
    }
    bool hasNewLayout(const std::string&) const override { return false; }
    void clearNewLayout(const std::string&) override {}
    void setSnapshotChangedCallback(LayoutSnapshotChangedCallback) override {}
    void setPropertyDecoder(std::shared_ptr<YogaPropertyDecoder>) override {}
};

}  // namespace agenui
