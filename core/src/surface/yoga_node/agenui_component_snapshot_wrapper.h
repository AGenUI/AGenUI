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

    /** Direct access for legacy code paths during the migration. */
    const ComponentSnapshot& raw() const { return *_snapshot; }
    ComponentSnapshot& mutableRaw() { return *_snapshot; }
    std::shared_ptr<ComponentSnapshot> sharedRaw() const { return _snapshot; }

    // ---------------- ILayoutDataWrapper ----------------

    const std::string& nodeId() const override;
    const std::string& rawId() const override;
    const std::string& componentType() const override;

    const std::vector<std::string>& childIds() const override;
    bool appendMode() const override;

    bool hasStyle(const std::string& key) const override;
    bool hasAttribute(const std::string& key) const override;

    std::string styleAsString(const std::string& key,
                              const std::string& def = std::string()) const override;
    double styleAsNumber(const std::string& key, double def = 0.0) const override;
    bool styleAsBool(const std::string& key, bool def = false) const override;

    std::string attributeAsString(const std::string& key,
                                  const std::string& def = std::string()) const override;
    double attributeAsNumber(const std::string& key, double def = 0.0) const override;
    bool attributeAsBool(const std::string& key, bool def = false) const override;

    void forEachStyle(ILayoutValueVisitor& visitor) const override;
    void forEachAttribute(ILayoutValueVisitor& visitor) const override;

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
