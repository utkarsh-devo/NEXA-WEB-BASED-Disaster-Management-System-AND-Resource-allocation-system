#pragma once
// ============================================================================
// NEXA :: configurable scoring weights (spec Modules 5 & 6:
// "the priority formula must be explicitly defined and configurable").
// Values come from config.json -> "custom_config.nexa" and can be changed at
// runtime from System Settings (PUT /api/settings/weights).
// ============================================================================
#include <string>
#include "../utilities/Errors.hpp"

namespace nexa::svc {

/// PRIORITY SCORE (0..100)
///   P = 100 * ( wS*S + wA*A + wU*U + wT*T + wSt*St + wTs*Ts ) / (sum of weights)
///   S  = severity / 5
///   A  = log10(1 + affected) / log10(1 + affectedSaturation)      capped at 1
///   U  = unmet units / required units                             (0 when fully resourced)
///   T  = min(1, ageHours / ageSaturationHours)                    older incidents rise (anti-starvation)
///   St = status factor: PENDING 1.0, ACTIVE 0.7, CONTAINED 0.25
///   Ts = category time-sensitivity (0..1, from incident_categories)
struct PriorityWeights {
    double severity = 0.35, affected = 0.20, unmet = 0.15, age = 0.10, status = 0.10, timeSensitivity = 0.10;
    double ageSaturationHours = 24.0;
    double affectedSaturation = 10000.0;
    double sum() const { return severity + affected + unmet + age + status + timeSensitivity; }
    void validate() const {
        if (severity < 0 || affected < 0 || unmet < 0 || age < 0 || status < 0 || timeSensitivity < 0) throw ValidationError("priority weights must be non-negative");
        if (sum() <= 0) throw ValidationError("at least one priority weight must be positive");
        if (ageSaturationHours <= 0 || affectedSaturation <= 0) throw ValidationError("saturation values must be positive");
    }
};

/// SUITABILITY SCORE (0..100) for resource r and incident i
///   Q = 100 * ( wC*C + wP*P + wAv*Av + wK*K ) / (sum of weights)
///   C  = |required capabilities ∩ resource capabilities| / |required|   (1 if none required)
///   P  = 1 / (1 + d / proximityScaleKm), d = road distance (or Haversine x detour factor if unroutable)
///   Av = available units / capacity
///   K  = min(1, available units / remaining units needed)
/// Hard filters (never ranked): not AVAILABLE, 0 units free, C = 0 when requirements exist.
struct SuitabilityWeights {
    double capability = 0.40, proximity = 0.30, availability = 0.15, capacity = 0.15;
    double proximityScaleKm = 10.0;
    double detourFactor = 1.3;   // used only when no road path exists
    double sum() const { return capability + proximity + availability + capacity; }
    void validate() const {
        if (capability < 0 || proximity < 0 || availability < 0 || capacity < 0) throw ValidationError("suitability weights must be non-negative");
        if (sum() <= 0) throw ValidationError("at least one suitability weight must be positive");
        if (proximityScaleKm <= 0 || detourFactor < 1) throw ValidationError("proximityScaleKm must be > 0 and detourFactor >= 1");
    }
};

/// REQUEST URGENCY SCORE (0..100)
///   R = 50*(urgency/5) + 20*min(1, log10(1+people)/3) + 30*(incident priority/100)
///   R >= escalationThreshold or urgency == 5 -> priority queue, otherwise FIFO queue.
struct EngineConfig {
    PriorityWeights priority;
    SuitabilityWeights suitability;
    double escalationThreshold = 70.0;
    int candidatePool = 30;          // KD-tree pre-selection size before ranking
    double averageSpeedKmh = 30.0;   // ETA when the road graph has no time weights
    std::string storage = "mysql";
};

} // namespace nexa::svc
