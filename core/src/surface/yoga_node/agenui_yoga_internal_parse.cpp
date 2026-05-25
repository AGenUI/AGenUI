#include "agenui_yoga_internal_parse.h"

#include <cstdlib>     // std::strtof (fallback when from_chars<float> is unavailable)
#include <string>

namespace agenui {
namespace yoga_internal {

namespace {

// Apple Clang on iOS toolchains historically lacked std::from_chars<float>
// up through libc++ 14. To keep this header portable across the bundled
// AppleClang and the GCC/Clang used by Android/Harmony, we implement
// parsing via std::strtof, which is C, fully noexcept, and reports failure
// through the endptr argument and errno.
//
// We deliberately avoid std::stof / std::stod / std::atof — they either
// throw on failure or have ambiguous error reporting.
inline float parseFloatNoexcept(const char* begin,
                                const char* end,
                                bool& ok) noexcept {
    ok = false;
    if (begin == end) return 0.0f;
    // strtof requires a NUL-terminated string; copy the slice to a small
    // local buffer (length is bounded by typical CSS token width).
    constexpr size_t kBufMax = 64;
    char buf[kBufMax];
    size_t len = static_cast<size_t>(end - begin);
    if (len == 0 || len >= kBufMax) return 0.0f;
    for (size_t i = 0; i < len; ++i) buf[i] = begin[i];
    buf[len] = '\0';

    char* endptr = nullptr;
    float result = std::strtof(buf, &endptr);
    if (endptr == buf) {
        // No characters consumed.
        return 0.0f;
    }
    ok = true;
    return result;
}

}  // namespace

float parseCssFloat(const std::string& token, bool* ok) noexcept {
    if (token.empty()) { if (ok) *ok = false; return 0.0f; }
    // Strip the unit suffix (anything beyond digits / sign / decimal-point).
    size_t unitPos = token.find_first_not_of("0123456789.-+");
    const char* begin = token.data();
    const char* end   = (unitPos != std::string::npos)
                            ? begin + unitPos
                            : begin + token.size();
    if (begin == end) { if (ok) *ok = false; return 0.0f; }

    bool localOk = false;
    float result = parseFloatNoexcept(begin, end, localOk);
    if (ok) *ok = localOk;
    return localOk ? result : 0.0f;
}

float parsePercent(const std::string& token, bool* ok) noexcept {
    if (token.empty() || token.back() != '%') {
        if (ok) *ok = false;
        return 0.0f;
    }
    return parseCssFloat(token.substr(0, token.size() - 1), ok);
}

}  // namespace yoga_internal
}  // namespace agenui
