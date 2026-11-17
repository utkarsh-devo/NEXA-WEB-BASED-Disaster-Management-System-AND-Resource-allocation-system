#pragma once
// ============================================================================
// NEXA :: Geo utilities
// Haversine great-circle distance and helpers used by the spatial engine.
//
// KD-Tree trick: a lat/lon point is mapped to a 3-D unit vector on the
// sphere. Straight-line (chord) distance between unit vectors is a strictly
// increasing function of great-circle distance, so a 3-D KD-Tree on those
// vectors returns the *exact* Haversine nearest neighbours, and the
// split-plane distance is a valid lower bound for pruning.
// ============================================================================
#include <cmath>

namespace nexa::geo {

constexpr double EARTH_RADIUS_KM = 6371.0088;   // mean Earth radius (IUGG)
constexpr double PI = 3.14159265358979323846;

inline double toRad(double deg) { return deg * PI / 180.0; }

inline bool validLatLon(double lat, double lon) {
    return std::isfinite(lat) && std::isfinite(lon) && lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0;
}

/// Great-circle distance in kilometres. O(1)
inline double haversineKm(double lat1, double lon1, double lat2, double lon2) {
    double dLat = toRad(lat2 - lat1), dLon = toRad(lon2 - lon1);
    double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
               std::cos(toRad(lat1)) * std::cos(toRad(lat2)) * std::sin(dLon / 2) * std::sin(dLon / 2);
    return 2 * EARTH_RADIUS_KM * std::asin(std::sqrt(a < 1 ? a : 1));
}

struct Vec3 { double x = 0, y = 0, z = 0; };

inline Vec3 toUnitVector(double lat, double lon) {
    double la = toRad(lat), lo = toRad(lon);
    return Vec3{std::cos(la) * std::cos(lo), std::cos(la) * std::sin(lo), std::sin(la)};
}
inline double chordToKm(double chord) { return 2 * EARTH_RADIUS_KM * std::asin(chord / 2 < 1 ? chord / 2 : 1); }
inline double kmToChord(double km)    { return 2 * std::sin(km / (2 * EARTH_RADIUS_KM)); }

} // namespace nexa::geo
