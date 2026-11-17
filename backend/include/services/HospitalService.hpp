#pragma once
// ============================================================================
// NEXA :: Module 10 — Hospital & Medical Coordination
// Registry + BST (by name) + KD-Tree (nearest) + Graph (accessibility) +
// sorting (rank) + per-hospital IntervalTree (bed reservation windows).
// Bed counters change only through reserve/release so capacity stays
// consistent with actual reservations (spec Module 10).
// ============================================================================
#include "ServiceContext.hpp"
#include "SpatialService.hpp"
#include "NetworkService.hpp"
#include "IncidentService.hpp"

namespace nexa::svc {

struct HospitalInput {
    std::string name, category, capabilities, contact, address;
    double lat = 0, lon = 0;
    int totalBeds = 0, availableBeds = -1, icuBeds = 0, icuAvailable = -1;
};

struct HospitalCandidate {
    Hospital hospital;
    double straightKm = 0, roadKm = 0, etaMinutes = 0;
    bool routable = true;
    double score = 0;
    std::string reason;
};

struct WindowLoad { long from = 0, until = 0; int reservedBeds = 0; int reservations = 0; };

class HospitalService {
public:
    HospitalService(ServiceContext& c, SpatialService& s, NetworkService& n, IncidentService& i, AuditService& a)
        : ctx_(c), spatial_(s), network_(n), incidents_(i), audit_(a) {}

    Hospital create(const HospitalInput& in, const std::string& actor);
    Hospital update(long id, const HospitalInput& in, const std::string& actor);
    void remove(long id, const std::string& actor);
    DynamicArray<Hospital> list(const std::string& sortBy);
    Hospital get(long id);
    DynamicArray<BedReservation> reservations(long hospitalId);

    /// Nearby suitable hospitals for a point or incident, ranked by road distance and spare capacity.
    DynamicArray<HospitalCandidate> suitable(double lat, double lon, const std::string& capability, int minBeds, std::size_t limit);
    DynamicArray<HospitalCandidate> suitableForIncident(long incidentId, const std::string& capability, int minBeds, std::size_t limit);

    BedReservation reserve(long hospitalId, long incidentId, int beds, double hours, const std::string& actor);
    BedReservation release(long reservationId, const std::string& actor);
    /// Interval-tree query: reservations overlapping [from, until].
    WindowLoad load(long hospitalId, long from, long until);

private:
    ServiceContext& ctx_;
    SpatialService& spatial_;
    NetworkService& network_;
    IncidentService& incidents_;
    AuditService& audit_;
    void validate(const HospitalInput& in);
};

} // namespace nexa::svc
