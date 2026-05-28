#pragma once

#include <string>

namespace agenui {

/**
 * @brief Lightweight value type for Yoga layout properties.
 * 
 * Replaces SerializableData in the layout conversion pipeline.
 * Supports the value types needed by CSS and A2UI layout properties:
 *   - Float (numeric values like 100.0, 20.5)
 *   - Bool (boolean values like true, false)
 *   - String (keywords like "auto", "flex-start", "center", or "100%")
 *   - None (invalid/unset value)
 * 
 * @remark This is a simple value object with no JSON dependency.
 *         ComponentSnapshotWrapper is responsible for converting
 *         SerializableData into YogaValue.
 */
class YogaValue {
public:
    /** Value type enumeration */
    enum Type {
        kNone,      ///< Invalid or unset value
        kFloat,     ///< Float value (e.g., 100.0, 20.5)
        kBool,      ///< Boolean value (true/false)
        kString     ///< String value (e.g., "auto", "flex-start", "100%")
    };

    /** Default constructor - creates invalid/none value */
    YogaValue();
    
    /** Constructor for float values */
    explicit YogaValue(float value);
    
    /** Constructor for bool values */
    explicit YogaValue(bool value);
    
    /** Constructor for string values (copy) */
    explicit YogaValue(const std::string& value);

    /** Constructor for string values (move) — avoids the copy when the caller
     *  already owns a temporary string. */
    explicit YogaValue(std::string&& value) noexcept;
    
    /** @return The value type */
    Type type() const { return _type; }
    
    /** @return True if this value is valid (not kNone) */
    bool isValid() const { return _type != kNone; }
    
    /** @return Float value (only valid when type() == kFloat) */
    float asFloat() const;
    
    /** @return Bool value (only valid when type() == kBool) */
    bool asBool() const;
    
    /** @return String value (valid when type() == kString) */
    const std::string& asString() const;

private:
    Type _type = kNone;
    float _float = 0.0f;
    bool _bool = false;
    std::string _string;
};

}  // namespace agenui
