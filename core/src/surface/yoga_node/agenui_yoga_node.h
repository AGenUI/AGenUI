#pragma once

#include <yoga/Yoga.h>
#include <functional>
#include <memory>
#include <vector>

namespace agenui {

class ILayoutDataWrapper;  // Forward declaration only

/**
 * @brief RAII wrapper for YGNodeRef
 *
 * Solves the problem of scattered raw YGNodeRef pointers with unclear lifetimes.
 * Each VirtualDOMNode holds one YogaNode instance.
 *
 * Features:
 *  - Auto YGNodeNew on construction, YGNodeFree on destruction
 *  - Maintains _hasOwner flag, replacing YGNodeGetOwner() check (CAPI has no equivalent)
 *  - setMeasureFunc accepts unified MeasureCallback, internally handles platform differences
 *  - createChild creates and attaches child nodes; child lifetime managed by this node
 */
class YogaNode {
public:
    /**
     * @brief Unified measure callback type (shields YGMeasureFunc signature differences)
     */
    using MeasureCallback = std::function<
        YGSize(float w, YGMeasureMode wMode, float h, YGMeasureMode hMode)>;

    YogaNode();
    ~YogaNode();

    // Non-copyable, movable
    YogaNode(const YogaNode&) = delete;
    YogaNode& operator=(const YogaNode&) = delete;
    YogaNode(YogaNode&& other) noexcept;
    YogaNode& operator=(YogaNode&& other) noexcept;

    /** Get underlying YGNodeRef (only for scenarios requiring direct Yoga API access) */
    YGNodeRef get() const { return _ygNode; }

    void setWidth(float w);
    void setHeight(float h);
    void setSize(float w, float h);

    /**
     * @brief Register measure callback (replaces YGNodeSetMeasureFunc)
     *
     * Internally adapts the unified MeasureCallback to YGMeasureFunc,
     * and sets context = this; on callback, restores to _measureCb.
     */
    void setMeasureFunc(MeasureCallback cb);

    /** Clear measure callback */
    void clearMeasureFunc();

    /** Mark layout dirty (trigger re-measurement) */
    void markDirty();

    /** Manually set hasNewLayout flag */
    void setHasNewLayout(bool val);

    /** Whether there is a new layout result */
    bool hasNewLayout() const;

    /** Clear hasNewLayout flag */
    void clearNewLayout();

    /**
     * @brief Apply styles and attributes from layout data wrapper to the Yoga node
     *
     * Equivalent to calling in sequence:
     *   CSSStyleConverter::convertToYoga(wrapper, get(), clearAfterConvert)
     *   A2UIAttributeConverter::convertToYoga(wrapper, get(), clearAfterConvert)
     *
     * @param wrapper          Layout data wrapper (input source; cleared of converted attributes when clearAfterConvert=true)
     * @param clearAfterConvert Whether to clear converted styles/attributes from wrapper, default true
     */
    void applySnapshot(ILayoutDataWrapper& wrapper, bool clearAfterConvert = true);

    /**
     * @brief Apply snapshot with Tabs-specific layout hints
     *
     * Wraps two Tabs rules on top of applySnapshot:
     *  1. If this node is a Tabs component, inject flex-grow:1 before applySnapshot (if not explicitly set)
     *  2. If this node's parent is a Tabs component, set this node to absolute layout after applySnapshot
     *     (top=kTabBarHeight, left=0, width=100%), removing it from flex flow
     *
     * Callers need not depend on TabsYogaHelper directly; all Tabs Yoga specialization logic is centralized here.
     *
     * @param wrapper          Layout data wrapper for this node
     * @param parentWrapper    Parent node's layout data wrapper (nullptr means no parent)
     * @param clearAfterConvert Whether to clear converted attributes from wrapper, default true
     */
    void applySnapshotWithTabsHints(ILayoutDataWrapper& wrapper,
                                    ILayoutDataWrapper& parentWrapper,
                                    bool clearAfterConvert = true);

    /**
     * @brief Insert child at position index
     * @note If child is already attached (_hasOwner == true), skip
     */
    void insertChild(YogaNode& child, uint32_t index);

    /**
     * @brief Remove child from this node
     */
    void removeChild(YogaNode& child);

    /** Get child count */
    uint32_t childCount() const;

    /**
     * @brief Check if this node is attached to a parent (replaces YGNodeGetOwner() != nullptr)
     */
    bool isAttached() const { return _hasOwner; }

    /**
     * @brief Create a child node, insert at index, lifetime managed by this node
     * @return Raw pointer to the new child (lifetime managed by this node)
     */
    YogaNode* createChild(uint32_t index);

    float getLayoutLeft()   const;
    float getLayoutTop()    const;
    float getLayoutWidth()  const;
    float getLayoutHeight() const;

    /**
     * @brief Restore width/height to the style values written by the last applySnapshot.
     *
     * Called when Surface size changes to release platform-locked dimensions,
     * allowing Yoga to re-layout using the original style (or Auto).
     */
    void resetToStyleSize();

    /**
     * @brief Snapshot the current YGValue width / height into private
     *        backup slots so that a later @ref resetToStyleSize() can
     *        restore them after the platform releases a forced size.
     *
     * Engines that drive the node via @ref YogaPropertyDecoder should
     * call this immediately after the decoder finishes (mirrors what
     * the legacy @ref applySnapshot did inline).
     */
    void saveCurrentStyleSize();

private:
    friend class YogaNodeManager;  // Allow YogaNodeManager to update _hasOwner
    static YGSize staticMeasureFunc(YGNodeRef node,
                                    float w, YGMeasureMode wMode,
                                    float h, YGMeasureMode hMode);

    YGNodeRef                             _ygNode    = nullptr;
    bool                                  _hasOwner  = false;
    MeasureCallback                       _measureCb;
    std::vector<std::unique_ptr<YogaNode>> _ownedChildren;  // Child nodes created via createChild

    bool    _hasSavedWidth  = false;
    YGValue _savedWidth     = {YGUndefined, YGUnitUndefined};
    bool    _hasSavedHeight = false;
    YGValue _savedHeight    = {YGUndefined, YGUnitUndefined};
};

}  // namespace agenui
