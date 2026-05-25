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

namespace agenui {

// Hard constraint #1: NO forward declaration of any concrete subclass here.
// The base interface must be free of concrete-type knowledge.

/**
 * @brief A read-only style/attribute value visitor.
 *
 * The decoder iterates style/attribute keys via @ref forEachStyle and
 * @ref forEachAttribute and reads each value through the visitor.
 * Returning false from the visitor stops iteration early.
 */
class ILayoutValueVisitor {
public:
    virtual ~ILayoutValueVisitor() = default;

    /** Called when a value is a string. */
    virtual bool onString(const std::string& key, const std::string& value) = 0;
    /** Called when a value is a number (double). */
    virtual bool onNumber(const std::string& key, double value) = 0;
    /** Called when a value is a boolean. */
    virtual bool onBool(const std::string& key, bool value) = 0;
    /** Called when a value is null / unsupported. */
    virtual bool onNull(const std::string& key) = 0;
};

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
    virtual const std::string& rawId() const = 0;
    virtual const std::string& componentType() const = 0;

    // ---------------- structure ----------------

    virtual const std::vector<std::string>& childIds() const = 0;
    virtual bool appendMode() const = 0;

    // ---------------- style / attribute access ----------------

    virtual bool hasStyle(const std::string& key) const = 0;
    virtual bool hasAttribute(const std::string& key) const = 0;

    virtual std::string styleAsString(const std::string& key,
                                      const std::string& def = std::string()) const = 0;
    virtual double styleAsNumber(const std::string& key, double def = 0.0) const = 0;
    virtual bool styleAsBool(const std::string& key, bool def = false) const = 0;

    virtual std::string attributeAsString(const std::string& key,
                                          const std::string& def = std::string()) const = 0;
    virtual double attributeAsNumber(const std::string& key, double def = 0.0) const = 0;
    virtual bool attributeAsBool(const std::string& key, bool def = false) const = 0;

    virtual void forEachStyle(ILayoutValueVisitor& visitor) const = 0;
    virtual void forEachAttribute(ILayoutValueVisitor& visitor) const = 0;

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
    // hard constraint #1 (base interface MUST NOT know any concrete subclass).
    // The hook has been removed; concrete decoders/engines that pair with a
    // specific wrapper subclass perform their own static_cast inside their
    // implementation files. See `core/src/surface/yoga_node/README.md`.
};

}  // namespace agenui
