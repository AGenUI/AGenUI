#pragma once

#include "../a2ui_component.h"

namespace a2ui {

/**
 * Card container: a COLUMN node whose looks come entirely from styles.
 *
 * The constructor deliberately applies no default chrome -- Card's defaults live in
 * core's per-component spec (border-radius: 16px) and arrive as ordinary styles.
 * Styling then goes through the shared A2UIComponent helpers, so Card behaves exactly
 * like Column / Row for every CSS key they support:
 *   - border-radius + overflow: resolved into the single NODE_CLIP decision by
 *     A2UIComponent::applyBorderStyles. Clipping is what makes the corners visible at
 *     all -- a radius on its own only rounds this node's background and leaves square
 *     children covering the corners.
 *   - border-width / border-color, background-color (incl. gradients), filter:
 *     drop-shadow(...) -- all read from styles by the shared helpers.
 *
 * Only "elevation" is still read from the top level, as a legacy alias for a vertical
 * drop shadow. Note that a top-level "radius" is NOT honoured (styles["border-radius"]
 * is the only radius input, and core always injects one); Android's CardComponent does
 * honour it, which is a known cross-platform divergence tracked separately.
 */
class CardComponent final : public A2UIComponent {
public:
    CardComponent(const std::string& id, const nlohmann::json& properties);
    ~CardComponent() override;

protected:
    void onUpdateProperties(const nlohmann::json& properties) override;

private:
    /** Parse the legacy elevation property and map it to NODE_CUSTOM_SHADOW. */
    void applyElevation(const nlohmann::json& properties);
};

} // namespace a2ui
