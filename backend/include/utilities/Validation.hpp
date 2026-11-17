#pragma once
// ============================================================================
// NEXA :: input validation helpers — throw ValidationError with a precise,
// user-facing message (shown in the UI form).
// ============================================================================
#include <string>
#include "Errors.hpp"
#include "Geo.hpp"
#include "Strings.hpp"

namespace nexa::validate {

inline void required(const std::string& v, const char* field) {
    if (util::trim(v).empty()) throw ValidationError(std::string(field) + " is required");
}
inline void maxLength(const std::string& v, std::size_t n, const char* field) {
    if (v.size() > n) throw ValidationError(std::string(field) + " must be at most " + std::to_string(n) + " characters");
}
inline void range(long v, long lo, long hi, const char* field) {
    if (v < lo || v > hi) throw ValidationError(std::string(field) + " must be between " + std::to_string(lo) + " and " + std::to_string(hi));
}
inline void nonNegative(double v, const char* field) {
    if (!(v >= 0)) throw ValidationError(std::string(field) + " must be non-negative");
}
inline void coordinates(double lat, double lon) {
    if (!geo::validLatLon(lat, lon)) throw ValidationError("Latitude must be in [-90, 90] and longitude in [-180, 180]");
    if (lat == 0 && lon == 0) throw ValidationError("Location is missing (0,0 is not accepted)");
}
inline void oneOf(const std::string& v, const char* const* options, int n, const char* field) {
    for (int i = 0; i < n; ++i) if (v == options[i]) return;
    std::string msg = std::string(field) + " must be one of:";
    for (int i = 0; i < n; ++i) msg += std::string(" ") + options[i];
    throw ValidationError(msg);
}

} // namespace nexa::validate
