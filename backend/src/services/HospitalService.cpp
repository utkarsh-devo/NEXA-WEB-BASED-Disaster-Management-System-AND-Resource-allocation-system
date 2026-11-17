#include "services/HospitalService.hpp"
#include "algorithms/Sorting.hpp"
#include "utilities/Geo.hpp"
#include "utilities/Validation.hpp"
#include <cmath>
#include <cstdio>

namespace nexa::svc {

void HospitalService::validate(const HospitalInput& in) {
    static const char* cats[] = {"general", "trauma", "burn", "pediatric", "multispecialty"};
    validate::required(in.name, "Hospital name");
    validate::maxLength(in.name, 160, "Hospital name");
    validate::oneOf(in.category, cats, 5, "Category");
    validate::coordinates(in.lat, in.lon);
    validate::range(in.totalBeds, 0, 20000, "Total beds");
    validate::range(in.icuBeds, 0, in.totalBeds, "ICU beds");
    if (in.availableBeds > in.totalBeds) throw ValidationError("Available beds cannot exceed total beds");
    if (in.icuAvailable > in.icuBeds) throw ValidationError("Available ICU beds cannot exceed ICU beds");
}

Hospital HospitalService::create(const HospitalInput& in, const std::string& actor) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    validate(in);
    Hospital h;
    h.name = util::trim(in.name); h.category = in.category; h.lat = in.lat; h.lon = in.lon;
    h.totalBeds = in.totalBeds; h.availableBeds = in.availableBeds < 0 ? in.totalBeds : in.availableBeds;
    h.icuBeds = in.icuBeds; h.icuAvailable = in.icuAvailable < 0 ? in.icuBeds : in.icuAvailable;
    h.capabilities = util::normaliseList(in.capabilities); h.contact = in.contact; h.address = in.address;
    h.updatedAt = util::nowEpoch();
    ctx_.repos.hospitals->insert(h);
    ctx_.store.hospitals.upsert(h);
    ctx_.store.hospitalByName.insert(util::lower(h.name), h.id);
    spatial_.indexHospital(h);
    audit_.record("AUDIT", "hospitals", "registered " + h.name, actor);
    return h;
}

Hospital HospitalService::update(long id, const HospitalInput& in, const std::string& actor) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    Hospital* cur = ctx_.store.hospitals.get(id);
    if (!cur) throw NotFoundError("Hospital not found");
    validate(in);
    int reserved = cur->totalBeds - cur->availableBeds;       // beds held by active reservations/admissions
    if (in.totalBeds < reserved) throw ConflictError("Total beds cannot drop below the " + std::to_string(reserved) + " currently reserved");
    Hospital before = *cur, h = *cur;
    h.name = util::trim(in.name); h.category = in.category; h.lat = in.lat; h.lon = in.lon;
    h.totalBeds = in.totalBeds;
    h.availableBeds = in.availableBeds < 0 ? in.totalBeds - reserved : in.availableBeds;
    if (h.availableBeds > h.totalBeds) h.availableBeds = h.totalBeds;
    h.icuBeds = in.icuBeds; h.icuAvailable = in.icuAvailable < 0 ? before.icuAvailable : in.icuAvailable;
    if (h.icuAvailable > h.icuBeds) h.icuAvailable = h.icuBeds;
    h.capabilities = util::normaliseList(in.capabilities); h.contact = in.contact; h.address = in.address;
    h.updatedAt = util::nowEpoch();
    ctx_.repos.hospitals->update(h);
    ctx_.store.hospitalByName.remove(util::lower(before.name));
    spatial_.unindexHospital(before);
    *cur = h;
    ctx_.store.hospitalByName.insert(util::lower(h.name), h.id);
    spatial_.indexHospital(h);
    audit_.record("AUDIT", "hospitals", "updated " + h.name, actor);
    return h;
}

void HospitalService::remove(long id, const std::string& actor) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    Hospital* cur = ctx_.store.hospitals.get(id);
    if (!cur) throw NotFoundError("Hospital not found");
    for (const auto& r : ctx_.store.reservations.items()) if (r.hospitalId == id) throw ConflictError("Hospital has reservation history and cannot be deleted");
    Hospital copy = *cur;
    ctx_.repos.hospitals->remove(id);
    ctx_.store.hospitalByName.remove(util::lower(copy.name));
    spatial_.unindexHospital(copy);
    ctx_.store.hospitals.remove(id);
    audit_.record("AUDIT", "hospitals", "deleted " + copy.name, actor);
}

DynamicArray<Hospital> HospitalService::list(const std::string& sortBy) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    DynamicArray<Hospital> out;
    if (sortBy == "beds") {
        out = ctx_.store.hospitals.items();
        algo::quickSort(out, [](const Hospital& a, const Hospital& b) { return a.availableBeds > b.availableBeds; });
    } else {
        // BST in-order traversal yields alphabetical order without sorting
        ctx_.store.hospitalByName.inorder([&](const std::string&, long id) { if (const Hospital* h = ctx_.store.hospitals.get(id)) out.pushBack(*h); });
    }
    return out;
}

Hospital HospitalService::get(long id) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    const Hospital* h = ctx_.store.hospitals.get(id);
    if (!h) throw NotFoundError("Hospital not found");
    return *h;
}

DynamicArray<BedReservation> HospitalService::reservations(long hospitalId) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    DynamicArray<BedReservation> out;
    ctx_.store.reservations.forEachOrdered([&](const BedReservation& r) { if (!hospitalId || r.hospitalId == hospitalId) out.pushBack(r); });
    return out;
}

DynamicArray<HospitalCandidate> HospitalService::suitable(double lat, double lon, const std::string& capability, int minBeds, std::size_t limit) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    if (!geo::validLatLon(lat, lon)) throw ValidationError("Invalid coordinates");
    SpatialFilter f; f.kind = "hospital"; f.capability = util::lower(capability); f.availableOnly = true;
    auto near = spatial_.nearest(lat, lon, 15, f);
    DynamicArray<double> roadKm, minutes; double snap = 0;
    int src = network_.distancesFrom(lat, lon, roadKm, minutes, snap);
    DynamicArray<HospitalCandidate> out;
    for (const auto& n : near) {
        const Hospital* h = ctx_.store.hospitals.get(n.id);
        if (!h || h->availableBeds < (minBeds > 0 ? minBeds : 1)) continue;
        HospitalCandidate c; c.hospital = *h; c.straightKm = n.distanceKm;
        double hs = 0; int v = src >= 0 ? spatial_.snapToVertex(h->lat, h->lon, &hs) : -1;
        if (v >= 0 && roadKm[v] != ds::Graph::INF) { c.roadKm = roadKm[v] + hs + snap; c.etaMinutes = minutes[v] + (hs + snap) / ctx_.config.averageSpeedKmh * 60; }
        else { c.routable = false; c.roadKm = c.straightKm * ctx_.config.suitability.detourFactor; c.etaMinutes = c.roadKm / ctx_.config.averageSpeedKmh * 60; }
        double spare = h->totalBeds ? (double)h->availableBeds / h->totalBeds : 0;
        c.score = std::round((60.0 / (1.0 + c.roadKm / 8.0) + 40.0 * spare) * 100) / 100;   // documented: 60% access, 40% spare capacity
        char buf[200];
        std::snprintf(buf, sizeof buf, "%.1f km %s (~%d min); %d/%d beds free; ICU %d/%d",
                      c.roadKm, c.routable ? "by road" : "est.", (int)std::round(c.etaMinutes), h->availableBeds, h->totalBeds, h->icuAvailable, h->icuBeds);
        c.reason = buf;
        out.pushBack(c);
    }
    algo::mergeSort(out, [](const HospitalCandidate& a, const HospitalCandidate& b) { return a.score > b.score; });
    DynamicArray<HospitalCandidate> cut;
    for (std::size_t i = 0; i < out.size() && i < limit; ++i) cut.pushBack(out[i]);
    return cut;
}

DynamicArray<HospitalCandidate> HospitalService::suitableForIncident(long incidentId, const std::string& capability, int minBeds, std::size_t limit) {
    Incident i = incidents_.get(incidentId);
    return suitable(i.lat, i.lon, capability, minBeds, limit);
}

BedReservation HospitalService::reserve(long hospitalId, long incidentId, int beds, double hours, const std::string& actor) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    Hospital* h = ctx_.store.hospitals.get(hospitalId);
    if (!h) throw NotFoundError("Hospital not found");
    const Incident* inc = ctx_.store.incidents.get(incidentId);
    if (!inc) throw NotFoundError("Incident not found");
    if (beds < 1) throw ValidationError("Beds must be at least 1");
    if (hours <= 0 || hours > 24 * 30) throw ValidationError("Reservation duration must be between 0 and 720 hours");
    if (beds > h->availableBeds) throw ConflictError(h->name + " has only " + std::to_string(h->availableBeds) + " bed(s) available");
    Hospital updated = *h;
    updated.availableBeds -= beds; updated.updatedAt = util::nowEpoch();
    BedReservation r; r.hospitalId = hospitalId; r.incidentId = incidentId; r.beds = beds;
    r.from = util::nowEpoch(); r.until = r.from + (long)(hours * 3600);
    IncidentEvent ev; ev.incidentId = incidentId; ev.type = "MEDICAL"; ev.actor = actor; ev.createdAt = r.from;
    ev.message = std::to_string(beds) + " bed(s) reserved at " + h->name;
    ctx_.repos.hospitals->commitReservation(updated, r, &ev);           // transaction
    *h = updated;
    ctx_.store.reservations.upsert(r);
    ctx_.store.windowsOf(ctx_.store.bedWindows, hospitalId).insert(ds::Interval{r.from, r.until, r.id});
    ctx_.store.timeline(incidentId).insertBack(ev);
    audit_.record("AUDIT", "hospitals", ev.message + " for incident #" + std::to_string(incidentId), actor);
    ctx_.sink.publish(LiveEvent{"hospital.updated", hospitalId, h->name, std::to_string(h->availableBeds) + " beds free", h->lat, h->lon});
    return r;
}

BedReservation HospitalService::release(long reservationId, const std::string& actor) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    BedReservation* r = ctx_.store.reservations.get(reservationId);
    if (!r) throw NotFoundError("Reservation not found");
    if (r->released) throw ConflictError("Reservation already released");
    Hospital* h = ctx_.store.hospitals.get(r->hospitalId);
    Hospital updated = *h;
    updated.availableBeds += r->beds;
    if (updated.availableBeds > updated.totalBeds) updated.availableBeds = updated.totalBeds;
    BedReservation rc = *r;
    rc.released = true;
    rc.until = util::nowEpoch() < rc.until ? util::nowEpoch() : rc.until;
    ctx_.repos.hospitals->commitRelease(updated, rc);
    *h = updated;
    auto& tree = ctx_.store.windowsOf(ctx_.store.bedWindows, rc.hospitalId);
    tree.remove(rc.id, rc.from);
    *r = rc;
    audit_.record("AUDIT", "hospitals", "released " + std::to_string(rc.beds) + " bed(s) at " + h->name, actor);
    ctx_.sink.publish(LiveEvent{"hospital.updated", h->id, h->name, std::to_string(h->availableBeds) + " beds free", h->lat, h->lon});
    return rc;
}

WindowLoad HospitalService::load(long hospitalId, long from, long until) {
    std::lock_guard<std::recursive_mutex> g(ctx_.store.mtx);
    if (!ctx_.store.hospitals.contains(hospitalId)) throw NotFoundError("Hospital not found");
    if (until < from) throw ValidationError("until must be >= from");
    WindowLoad w; w.from = from; w.until = until;
    for (const auto& iv : ctx_.store.windowsOf(ctx_.store.bedWindows, hospitalId).allOverlaps(from, until)) {
        if (const BedReservation* r = ctx_.store.reservations.get(iv.id)) { w.reservedBeds += r->beds; ++w.reservations; }
    }
    return w;
}

} // namespace nexa::svc
