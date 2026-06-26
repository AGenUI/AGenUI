#pragma once

/**
 * @file agenui_layout_data_wrapper.h
 * @brief Public SDK contract for the layout-data abstraction.
 *
 * SDK consumers subclass ILayoutDataWrapper to feed their own
 * style/attribute model into the layout engine. The interface stays
 * deliberately minimal.
 *
 * Hard constraint #1 (zero business-type leak): No business types such as
 * ComponentSnapshot are allowed to appear in any public API of the layout
 * engine, layout delegate, decoders or measurement layer. All access to
 * the underlying business data must go through this wrapper interface.
 *
 * Hard constraint #2 (decoder + wrapper are mandatory): the wrapper is one
 * of the two mandatory components of the new layout pipeline; the other one
 * is the decoder family (YogaPropertyDecoder and its subclasses) which
 * consumes ILayoutDataWrapper and writes pure layout commands to the engine.
 *
 * @par Design rule: NO concrete-type hooks
 *   This interface MUST NOT contain `asXxx()` downcast hooks or any
 *   forward declaration of concrete subclasses. If a concrete decoder
 *   implementation needs typed access to its paired wrapper, the cast
 *   MUST live inside the decoder's own apply() body — the consumer
 *   that owns the wrapper/decoder pairing is responsible for the
 *   safety of that cast. Adding any `asXxx()` hook here breaks the
 *   "zero business-type leak" contract for SDK consumers.
 *
 * @par Lifetime requirement: shared_ptr ownership is mandatory
 *   ILayoutDataWrapper inherits std::enable_shared_from_this so that the
 *   layout engine can capture a `weak_from_this()` inside Yoga measure
 *   callbacks (which may fire on the layout thread after the wrapper has
 *   been replaced). SDK consumers MUST own every wrapper instance via
 *   std::shared_ptr (e.g. std::make_shared<MyWrapper>(...)) before passing
 *   it into the engine. Holding a wrapper through a raw pointer or a
 *   stack value violates weak_from_this()'s preconditions and silently
 *   produces empty weak refs (measure callbacks will fall back to
 *   YGUndefined sizing).
 *
 * @par SDK consumers
 *   Include via:
 *       \#include "surface/yoga_node/agenui_layout_data_wrapper.h"
 *   See `core/src/surface/yoga_node/README.md` for the extension model.
 */

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <functional>

#include "surface/yoga_node/agenui_yoga_value.h"

namespace agenui {

// Phase A hard constraint 1: NO forward declaration of any concrete subclass here.
// The base interface must be free of concrete-type knowledge.

/**
 * @brief Layout data wrapper interface.
 *
 * Implemented by ComponentSnapshotWrapper to expose only the fields the
 * layout engine and decoders need, without leaking ComponentSnapshot.
 *
 * Threading: same as VirtualDOM (message thread); no internal locks.
 */
class ILayoutDataWrapper
    : public std::enable_shared_from_this<ILayoutDataWrapper> {
public:
    virtual ~ILayoutDataWrapper() = default;

    // ---------------- identification ----------------

    virtual const std::string& nodeId() const = 0;
    virtual const std::string& componentType() const = 0;

    // ---------------- structure ----------------

    virtual const std::vector<std::string>& childIds() const = 0;

    // ---------------- style / attribute access ----------------

    virtual std::string styleAsString(const std::string& key,
                                      const std::string& def = std::string()) const = 0;

    /**
     * @brief Get style value as YogaValue.
     * @return YogaValue (kFloat / kBool / kString) for valid values; YogaValue() (kNone) otherwise.
     */
    virtual YogaValue getStyleValue(const std::string& key) const = 0;

    /**
     * @brief Get attribute value as YogaValue.
     * @return YogaValue (kFloat / kBool / kString) for valid values; YogaValue() (kNone) otherwise.
     */
    virtual YogaValue getAttributeValue(const std::string& key) const = 0;

    virtual void clearStyle(const std::string& key) = 0;
    virtual void clearAttribute(const std::string& key) = 0;

    // ---------------- platform-size lock ----------------

    virtual bool platformSizeLocked() const = 0;
    virtual void setPlatformSizeLocked(bool locked) = 0;

    // ---------------- measurement ----------------

    virtual std::string serializeForMeasure() const = 0;

    // ---------------- layout result writeback ----------------

    virtual void applyLayoutResult(float x, float y,
                                   float width, float height,
                                   int countOfLines = -1) = 0;

    // NOTE: previously this interface exposed `asComponentSnapshotWrapper()`
    // as a manual-RTTI hook so engine/decoders could downcast. That violated
    // Phase A hard constraint 1 (base interface MUST NOT know any concrete subclass).
    // The hook has been removed; concrete decoders/engines that pair with a
    // specific wrapper subclass perform their own static_cast inside their
    // implementation files. See `core/src/surface/yoga_node/README.md`.
};

}  // namespace agenui
