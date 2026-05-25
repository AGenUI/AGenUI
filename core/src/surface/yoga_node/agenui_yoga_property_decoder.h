#pragma once

/**
 * @file agenui_yoga_property_decoder.h
 * @brief Public SDK contract for property decoders that drive Yoga.
 *
 * A YogaPropertyDecoder reads style + attribute entries from an
 * @ref ILayoutDataWrapper and writes Yoga style commands to the given
 * Yoga node. There is **one** decoder per layout engine instance —
 * injected via @ref ILayoutDelegate::setPropertyDecoder (the engine
 * falls back to a bundled default if none is set).
 *
 * Hard constraint #2 (decoder + wrapper are mandatory): every Yoga style
 * / attribute write goes through a YogaPropertyDecoder subclass. The
 * subclass reads exclusively from @ref ILayoutDataWrapper; if it needs
 * concrete-typed access to its paired wrapper subclass, the cast lives
 * inside the subclass's `apply()` body (hard constraint #2).
 *
 * @par SDK consumers
 *   Subclass YogaPropertyDecoder, instantiate as a `shared_ptr`, and
 *   register it via `ILayoutDelegate::setPropertyDecoder(...)`. The
 *   engine will route every `onSnapshotChanged` through your decoder.
 *   Include via:
 *       \#include "surface/yoga_node/agenui_yoga_property_decoder.h"
 *   See `core/src/surface/yoga_node/README.md` for a minimal example.
 */

#include <yoga/Yoga.h>

namespace agenui {

class ILayoutDataWrapper;

/**
 * @brief Single property decoder contract.
 *
 * Subclasses implement @ref apply, which reads from a wrapper and writes
 * Yoga style commands to the given Yoga node. A single decoder is
 * responsible for the entire property set (CSS styles + A2UI attributes
 * + any vendor extensions) — there is no "one decoder per property
 * domain" model.
 */
class YogaPropertyDecoder {
public:
    virtual ~YogaPropertyDecoder() = default;

    /**
     * @brief Apply every supported key from @p wrapper to @p node.
     *
     * @param wrapper          source data
     * @param node             target Yoga node
     * @param clearAfterDecode whether to drop consumed keys from the
     *                         wrapper after decoding (mirrors the legacy
     *                         clearAfterConvert semantics)
     */
    virtual void apply(ILayoutDataWrapper& wrapper,
                       YGNodeRef node,
                       bool clearAfterDecode = true) = 0;
};

/**
 * @brief Bundled default decoder.
 *
 * Sequentially routes through the legacy CSS converter then the A2UI
 * converter. Used by @ref YogaLayoutEngine when no custom decoder has
 * been registered via @ref ILayoutDelegate::setPropertyDecoder.
 *
 * SDK consumers can either:
 *   - leave it in place and let the engine use this default, OR
 *   - subclass @ref YogaPropertyDecoder and register their own via
 *     `ILayoutDelegate::setPropertyDecoder(...)`.
 */
class BuiltinYogaPropertyDecoder final : public YogaPropertyDecoder {
public:
    void apply(ILayoutDataWrapper& wrapper,
               YGNodeRef node,
               bool clearAfterDecode = true) override;
};

}  // namespace agenui
