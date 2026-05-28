#pragma once

/**
 * @file agenui_component_snapshot_wrapper.h
 * @brief ComponentSnapshot wrapper that implements ILayoutDataWrapper.
 *
 * This is the only place where ComponentSnapshot is allowed to be touched
 * inside the layout pipeline. Engine / decoders / delegate must only see
 * @ref ILayoutDataWrapper.
 */

#include "surface/yoga_node/agenui_layout_data_wrapper.h"
#include "surface/yoga_node/agenui_yoga_value.h"
#include "surface/virtual_dom/agenui_component_snapshot.h"

#include <memory>

namespace agenui {

/**
 * @brief Wraps a shared ComponentSnapshot so that the layout pipeline can
 *        access it without learning the business type.
 */
class ComponentSnapshotWrapper final : public ILayoutDataWrapper {
public:
    explicit ComponentSnapshotWrapper(std::shared_ptr<ComponentSnapshot> snapshot);
    ~ComponentSnapshotWrapper() override;

    ComponentSnapshotWrapper(const ComponentSnapshotWrapper&) = delete;
    ComponentSnapshotWrapper& operator=(const ComponentSnapshotWrapper&) = delete;

    /**
     * @brief Get style value as YogaValue (for converters).
     * @return YogaValue representing the style value, or YogaValue() if key doesn't exist.
     */
    YogaValue getStyleValue(const std::string& key) const override;

    /**
     * @brief Get attribute value as YogaValue (for converters).
     * @return YogaValue representing the attribute value, or YogaValue() if key doesn't exist.
     */
    YogaValue getAttributeValue(const std::string& key) const override;

    // ---------------- ILayoutDataWrapper ----------------

    const std::string& nodeId() const override;
    const std::string& componentType() const override;

    const std::vector<std::string>& childIds() const override;

    std::string styleAsString(const std::string& key,
                              const std::string& def = std::string()) const override;

    void clearStyle(const std::string& key) override;
    void clearAttribute(const std::string& key) override;

    bool platformSizeLocked() const override { return _platformSizeLocked; }
    void setPlatformSizeLocked(bool locked) override { _platformSizeLocked = locked; }

    std::string serializeForMeasure() const override;

    void applyLayoutResult(float x, float y,
                           float width, float height,
                           int countOfLines = -1) override;

private:
    std::shared_ptr<ComponentSnapshot> _snapshot;
    bool _platformSizeLocked = false;
};

}  // namespace agenui
