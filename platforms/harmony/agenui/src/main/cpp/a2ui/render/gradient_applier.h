#pragma once

#include <arkui/native_node.h>
#include "style_parser/agenui_color_parser.h"

namespace a2ui {

/**
 * Applies a parsed CSS gradient (linear / radial / conic) to an ArkUI node by
 * writing the matching NDK attribute (NODE_LINEAR_GRADIENT / NODE_RADIAL_GRADIENT /
 * NODE_SWEEP_GRADIENT). Mirrors the Android GradientDrawableFactory algorithms so
 * the three platforms render the same JSON identically.
 *
 * Geometry that depends on view size (radial center / radius / sweep center) is
 * resolved against the supplied a2ui-unit width/height; the values are converted
 * to vp internally.
 */
class GradientApplier {
public:
    /**
     * Write the gradient described by `info` to `node`.
     *
     * @param node              ArkUI node handle (must be valid).
     * @param info              Parsed gradient info from agenui::ColorParser::parse.
     * @param viewWidthA2ui     View width in a2ui units (used to resolve % / size keywords).
     * @param viewHeightA2ui    View height in a2ui units.
     */
    static void apply(ArkUI_NodeHandle node,
                      const agenui::GradientInfo& info,
                      float viewWidthA2ui,
                      float viewHeightA2ui);

    /**
     * Reset all three gradient attributes on the node so a switch back to a solid
     * color does not leave stale gradient state behind.
     */
    static void reset(ArkUI_NodeHandle node);
};

}  // namespace a2ui
