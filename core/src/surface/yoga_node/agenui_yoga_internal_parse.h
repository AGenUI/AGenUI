#pragma once

/**
 * @file agenui_yoga_internal_parse.h
 * @brief Internal noexcept parsers for CSS-like length / percent tokens.
 *
 * Project requirement: NO C++ exceptions. Implementations in the .cpp use
 * `std::from_chars` (C++17, noexcept) and report failure via the bool*
 * out-parameter. Callers can also pass nullptr to ignore the success flag.
 *
 * NOT exposed via public SDK headers — strictly an internal helper.
 */

#include <string>

namespace agenui {
namespace yoga_internal {

/**
 * @brief Parse a CSS-like length token ("8px", "1.5", "12vp", "-3").
 *
 * The implementation strips any non-numeric trailing characters before
 * delegating to std::from_chars. Returns 0.0f and sets *ok=false when the
 * numeric prefix is empty or unparseable. Never throws.
 */
float parseCssFloat(const std::string& token, bool* ok = nullptr) noexcept;

/**
 * @brief Parse a percent token ("50%"). Returns 0.0f and *ok=false if the
 * suffix isn't '%' or the prefix isn't numeric. Never throws.
 */
float parsePercent(const std::string& token, bool* ok = nullptr) noexcept;

}  // namespace yoga_internal
}  // namespace agenui
