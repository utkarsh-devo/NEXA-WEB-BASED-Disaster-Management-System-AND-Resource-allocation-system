// ============================================================================
// NEXA :: demo dataset (Delhi NCR) — ALL DATA IS SIMULATED.
// Place names and approximate coordinates are real so the map is
// meaningful; capacities, incidents, units and road travel times are
// invented for demonstration and must never be treated as real emergency
// information (spec rule 18).
// The same seeder feeds MySQL (first start) and the in-memory demo mode,
// and tools/export_seed_sql generates database/seed/seed_demo.sql from it.
// ============================================================================
#include "services/NexaCore.hpp"
#include "utilities/Geo.hpp"
#include <cmath>

namespace nexa::svc {

namespace {

struct Cat { int id, parent; const char* name; const char* code; double ts; const char* caps; };
const Cat CATEGORIES[] = {
    {1, 0, "Natural", "NATURAL", 0.7, "rescue,medical"},
    {2, 0, "Technological", "TECH", 0.9, "fire_suppression,medical"},
    {3, 0, "Societal", "SOCIETAL", 0.5, "medical"},
    {11, 1, "Earthquake", "EARTHQUAKE", 0.9, "rescue,heavy_lifting,medical"},
    {12, 1, "Flood", "FLOOD", 0.8, "water_rescue,evacuation,medical"},
    {13, 1, "Cyclone / Storm", "STORM", 0.7, "evacuation,shelter,rescue"},
    {14, 1, "Landslide", "LANDSLIDE", 0.8, "rescue,heavy_lifting"},
    {15, 1, "Heatwave", "HEATWAVE", 0.5, "medical,water_supply"},
    {21, 2, "Urban Fire", "FIRE", 1.0, "fire_suppression,medical"},
    {22, 2, "Industrial Fire", "IND_FIRE", 1.0, "fire_suppression,hazmat,medical"},
    {23, 2, "Gas Leak / Hazmat", "HAZMAT", 0.95, "hazmat,evacuation,medical"},
    {24, 2, "Building Collapse", "COLLAPSE", 0.95, "rescue,heavy_lifting,medical"},
    {25, 2, "Mass-Casualty Road Accident", "ROAD_MCI", 0.9, "medical,extrication"},
    {31, 3, "Stampede / Crowd Crush", "STAMPEDE", 0.9, "medical,crowd_control"},
    {32, 3, "Epidemic Outbreak", "EPIDEMIC", 0.4, "medical"},
    {33, 3, "Water / Food Shortage", "SHORTAGE", 0.3, "food_water,shelter"},
};

struct Type { int id; const char* name; const char* category; const char* caps; };
const Type TYPES[] = {
    {1, "Advanced Life Support Ambulance", "ambulance", "medical,trauma,transport"},
    {2, "Basic Life Support Ambulance", "ambulance", "medical,transport"},
    {3, "Fire Tender", "fire_engine", "fire_suppression,water_supply"},
    {4, "Hazmat Response Unit", "special_team", "hazmat,decontamination,fire_suppression"},
    {5, "Heavy Rescue Vehicle", "rescue_vehicle", "rescue,heavy_lifting,extrication"},
    {6, "Water Rescue Boat Team", "rescue_vehicle", "water_rescue,evacuation"},
    {7, "Medical Supply Cache", "medical_supplies", "medical"},
    {8, "Rescue Equipment Kit", "rescue_equipment", "rescue,extrication"},
    {9, "Food & Water Stock", "food_water", "food_water,water_supply"},
    {10, "Emergency Shelter", "shelter", "shelter,evacuation"},
    {11, "Rescue Personnel Squad", "personnel", "rescue,crowd_control,evacuation"},
    {12, "Disaster Response Team (NDRF-style)", "special_team", "rescue,water_rescue,heavy_lifting,hazmat,medical"},
};

struct Place { const char* name; const char* kind; double lat, lon; };
const Place PLACES[] = {
    {"Connaught Place", "junction", 28.6315, 77.2167},   // 0
    {"ITO", "junction", 28.6280, 77.2410},               // 1
    {"India Gate", "junction", 28.6129, 77.2295},        // 2
    {"AIIMS Flyover", "junction", 28.5680, 77.2090},     // 3
    {"Dhaula Kuan", "junction", 28.5918, 77.1615},       // 4
    {"Karol Bagh", "junction", 28.6519, 77.1909},        // 5
    {"Kashmere Gate", "junction", 28.6675, 77.2280},     // 6
    {"Lajpat Nagar", "junction", 28.5677, 77.2433},      // 7
    {"Nehru Place", "junction", 28.5491, 77.2533},       // 8
    {"Saket", "junction", 28.5245, 77.2066},             // 9
    {"Mehrauli", "junction", 28.5244, 77.1855},          // 10
    {"Vasant Kunj", "junction", 28.5200, 77.1590},       // 11
    {"IGI Airport T3", "junction", 28.5562, 77.1000},    // 12
    {"Dwarka Sector 21", "junction", 28.5523, 77.0583},  // 13
    {"Janakpuri", "junction", 28.6219, 77.0878},         // 14
    {"Punjabi Bagh", "junction", 28.6683, 77.1250},      // 15
    {"Rohini", "junction", 28.7383, 77.0822},            // 16
    {"Pitampura", "junction", 28.7033, 77.1310},         // 17
    {"Azadpur", "junction", 28.7075, 77.1800},           // 18
    {"Civil Lines", "junction", 28.6814, 77.2226},       // 19
    {"Shahdara", "junction", 28.6735, 77.2896},          // 20
    {"Laxmi Nagar", "junction", 28.6304, 77.2777},       // 21
    {"Akshardham", "junction", 28.6127, 77.2773},        // 22
    {"Mayur Vihar", "junction", 28.6043, 77.2944},       // 23
    {"Anand Vihar", "junction", 28.6469, 77.3159},       // 24
    {"Noida Sector 18", "junction", 28.5708, 77.3260},   // 25
    {"Noida Sector 62", "junction", 28.6270, 77.3727},   // 26
    {"Botanical Garden", "junction", 28.5641, 77.3341},  // 27
    {"Pari Chowk, Greater Noida", "junction", 28.4665, 77.5130}, // 28
    {"Raj Nagar, Ghaziabad", "junction", 28.6692, 77.4538},      // 29
    {"Indirapuram", "junction", 28.6460, 77.3695},       // 30
    {"Cyber City, Gurugram", "junction", 28.4950, 77.0890},      // 31
    {"MG Road, Gurugram", "junction", 28.4796, 77.0800}, // 32
    {"Sohna Road, Gurugram", "junction", 28.4170, 77.0440},      // 33
    {"Faridabad Sector 16", "junction", 28.4089, 77.3178},       // 34
    {"Badarpur", "junction", 28.5032, 77.3006},          // 35
    {"Okhla", "junction", 28.5355, 77.2750},             // 36
    {"Kalindi Kunj", "junction", 28.5452, 77.3093},      // 37
    {"Noida Expressway Sector 93", "junction", 28.5180, 77.3870}, // 38
    {"Yamuna Khadar Relief Camp", "shelter", 28.6420, 77.2630},  // 39 (reached only by a pontoon road)
    {"Pragati Maidan", "junction", 28.6180, 77.2460},    // 40
};

// road list: (from index, to index, road class) class 0 = arterial 28 km/h, 1 = expressway 55 km/h, 2 = local 18 km/h
struct RoadDef { int a, b, cls; };
const RoadDef ROADS[] = {
    {0, 1, 0}, {0, 2, 0}, {0, 5, 0}, {0, 6, 0}, {1, 2, 0}, {1, 40, 0}, {40, 22, 0}, {2, 3, 0}, {3, 4, 0}, {3, 7, 0},
    {3, 9, 0}, {4, 12, 1}, {4, 5, 0}, {4, 14, 0}, {5, 15, 0}, {5, 6, 0}, {6, 19, 0}, {6, 20, 0}, {7, 8, 0}, {7, 36, 0},
    {8, 9, 0}, {8, 36, 0}, {9, 10, 0}, {10, 11, 0}, {11, 12, 0}, {12, 13, 1}, {13, 14, 0}, {14, 15, 0}, {15, 17, 0},
    {16, 17, 0}, {16, 15, 1}, {17, 18, 0}, {18, 19, 0}, {19, 20, 0}, {20, 21, 0}, {21, 22, 0}, {1, 21, 0}, {22, 23, 0},
    {21, 24, 0}, {23, 24, 0}, {23, 25, 1}, {24, 30, 0}, {30, 26, 0}, {26, 25, 1}, {25, 27, 0}, {27, 37, 0}, {37, 36, 0},
    {37, 35, 0}, {35, 34, 1}, {36, 35, 0}, {27, 38, 1}, {38, 28, 1}, {30, 29, 1}, {29, 20, 1}, {24, 29, 1}, {11, 31, 1},
    {31, 32, 0}, {32, 33, 0}, {31, 12, 1}, {33, 34, 2}, {10, 32, 1}, {2, 40, 0}, {22, 39, 2},
};
const int BLOCKED_ROAD_INDEX = 62;  // {22, 39} pontoon road — seeded as blocked -> disconnected relief camp

struct HospDef { const char* name; const char* cat; double lat, lon; int beds, avail, icu, icuAvail; const char* caps; };
const HospDef HOSPITALS[] = {
    {"AIIMS Trauma Centre", "trauma", 28.5672, 77.2058, 260, 54, 40, 9, "trauma,icu,burn,neuro,pediatric"},
    {"Safdarjung Hospital", "general", 28.5685, 77.2066, 300, 71, 30, 6, "trauma,burn,icu,general"},
    {"Ram Manohar Lohia Hospital", "general", 28.6256, 77.2004, 180, 38, 20, 5, "general,icu,cardiac"},
    {"Lok Nayak Hospital", "general", 28.6390, 77.2370, 220, 45, 24, 7, "trauma,general,icu"},
    {"GTB Hospital", "general", 28.6845, 77.3093, 210, 62, 18, 4, "trauma,general,burn"},
    {"Fortis Hospital Noida", "multispecialty", 28.6181, 77.3720, 140, 30, 22, 6, "cardiac,icu,trauma"},
    {"Max Super Speciality Saket", "multispecialty", 28.5274, 77.2131, 160, 22, 26, 3, "cardiac,icu,neuro,trauma"},
    {"Medanta Gurugram", "multispecialty", 28.4395, 77.0405, 200, 41, 35, 8, "cardiac,icu,trauma,neuro"},
    {"Sir Ganga Ram Hospital", "multispecialty", 28.6381, 77.1895, 170, 26, 24, 5, "icu,cardiac,pediatric"},
    {"District Hospital Noida", "general", 28.5799, 77.3420, 120, 48, 10, 4, "general,trauma"},
    {"District Hospital Ghaziabad", "general", 28.6650, 77.4400, 110, 36, 8, 2, "general,trauma"},
    {"BK Civil Hospital Faridabad", "general", 28.3920, 77.3080, 120, 40, 8, 3, "general,trauma"},
    {"Deen Dayal Upadhyay Hospital", "general", 28.6317, 77.1120, 150, 52, 12, 3, "general,trauma,burn"},
    {"Kalawati Saran Children's Hospital", "pediatric", 28.6363, 77.2083, 90, 25, 12, 4, "pediatric,icu"},
};

// resource stations: (place index, organisation, list of type ids with counts)
struct Unit { int type; int capacity; };
struct StationDef { int place; const char* org; const char* prefix; Unit units[6]; int n; };
const StationDef STATIONS[] = {
    {0, "Delhi Fire Service — CP", "CP", {{3, 1}, {3, 1}, {1, 1}, {8, 6}}, 4},
    {6, "Delhi Fire Service — Kashmere Gate", "KG", {{3, 1}, {5, 1}, {2, 1}, {11, 12}}, 4},
    {3, "CATS Ambulance — AIIMS", "AI", {{1, 1}, {1, 1}, {2, 1}, {7, 40}}, 4},
    {9, "Delhi Fire Service — Saket", "SK", {{3, 1}, {1, 1}, {8, 4}}, 3},
    {12, "Airport Emergency Services", "AP", {{4, 1}, {3, 1}, {1, 1}}, 3},
    {13, "Dwarka Disaster Depot", "DW", {{5, 1}, {9, 200}, {10, 120}, {2, 1}}, 4},
    {16, "Rohini Fire & Rescue", "RH", {{3, 1}, {2, 1}, {11, 10}}, 3},
    {20, "Shahdara Civil Defence", "SH", {{11, 15}, {2, 1}, {9, 150}}, 3},
    {21, "Yamuna Flood Control Post", "YF", {{6, 1}, {6, 1}, {12, 1}}, 3},
    {25, "Noida Fire Station Sector 18", "NS", {{3, 1}, {1, 1}, {5, 1}, {2, 1}}, 4},
    {26, "Noida Sector 62 Response Base", "N6", {{4, 1}, {1, 1}, {7, 30}}, 3},
    {29, "Ghaziabad Fire Service", "GZ", {{3, 1}, {2, 1}, {11, 8}}, 3},
    {31, "Gurugram Fire Station Cyber City", "GG", {{3, 1}, {5, 1}, {1, 1}}, 3},
    {33, "Gurugram Disaster Depot", "GD", {{10, 80}, {9, 120}, {2, 1}}, 3},
    {34, "Faridabad Fire & Ambulance", "FB", {{3, 1}, {2, 1}, {6, 1}}, 3},
    {36, "Okhla Industrial Response Unit", "OK", {{4, 1}, {3, 1}, {12, 1}}, 3},
    {28, "Greater Noida Response Base", "GN", {{3, 1}, {1, 1}, {10, 60}}, 3},
};

const char* typePrefix(int type) {
    switch (type) {
        case 1: return "ALS"; case 2: return "BLS"; case 3: return "FT"; case 4: return "HZ"; case 5: return "HRV"; case 6: return "BOAT";
        case 7: return "MED"; case 8: return "KIT"; case 9: return "FW"; case 10: return "SHL"; case 11: return "SQD"; default: return "DRT";
    }
}

struct IncDef { int cat; const char* title; const char* desc; int sev; int place; double dLat, dLon; long affected; const char* caps; int units; double hoursAgo; };
const IncDef INCIDENTS[] = {
    {24, "Residential building collapse", "Four-storey building collapsed after heavy rain weakened the foundation; at least 12 people believed trapped under debris", 5, 5, 0.004, -0.003, 60, "rescue,heavy_lifting,medical", 4, 1.5},
    {22, "Chemical warehouse fire", "Fire in a solvent storage unit; thick black smoke, risk of explosion from stored drums", 5, 36, -0.003, 0.004, 120, "fire_suppression,hazmat,medical", 4, 0.8},
    {12, "Yamuna floodplain inundation", "Water level above danger mark; families stranded in low-lying settlements near the river bank", 4, 21, 0.006, -0.010, 900, "water_rescue,evacuation,medical", 3, 5.0},
    {25, "Multi-vehicle pile-up on expressway", "Fog-related collision involving a bus and five cars; multiple casualties reported", 4, 38, 0.002, 0.003, 35, "medical,extrication", 3, 0.6},
    {23, "Ammonia leak at cold storage", "Ammonia leak from a refrigeration plant; residents complaining of eye irritation and breathing difficulty", 4, 30, -0.004, 0.003, 300, "hazmat,evacuation,medical", 2, 2.2},
    {21, "Market fire", "Fire spreading through shops in a crowded market lane; access for tenders is narrow", 3, 7, 0.002, 0.001, 80, "fire_suppression,medical", 2, 3.1},
    {31, "Crowd crush at railway station", "Overcrowding on a foot-over bridge led to a crush; several injured", 4, 6, -0.002, 0.004, 150, "medical,crowd_control", 3, 0.4},
    {15, "Heat-stroke cluster", "Multiple heat-stroke cases among construction workers at a site", 2, 31, 0.003, 0.002, 40, "medical,water_supply", 1, 6.5},
    {33, "Drinking water shortage in relief camp", "Relief camp reports shortage of potable water and food packets for displaced families", 3, 39, 0.001, 0.001, 450, "food_water,shelter", 2, 9.0},
    {32, "Suspected waterborne illness outbreak", "Cluster of gastroenteritis cases reported in a resettlement colony after flooding", 3, 20, 0.004, 0.005, 220, "medical", 1, 12.0},
    {14, "Retaining wall collapse", "Retaining wall collapsed onto a service lane; vehicles buried, two people missing", 4, 11, -0.003, -0.002, 12, "rescue,heavy_lifting", 2, 1.1},
    {13, "Storm damage — trees and power lines down", "Severe thunderstorm uprooted trees; roads blocked and live wires on the road", 2, 17, 0.002, -0.003, 90, "rescue,evacuation", 1, 4.0},
    {21, "Factory boiler explosion", "Boiler explosion followed by fire at a small manufacturing unit", 5, 29, 0.003, 0.004, 45, "fire_suppression,medical,rescue", 3, 0.3},
    {12, "Underpass waterlogging", "Bus stranded in a flooded underpass with passengers on board", 3, 2, -0.002, 0.003, 40, "water_rescue,evacuation", 1, 1.9},
};

} // namespace

bool NexaCore::seedDemoData() {
    std::lock_guard<std::recursive_mutex> g(store.mtx);
    const std::string actor = "seeder";
    // reference data (needed even when operational data exists)
    if (repos.categories->findAll().empty())
        for (const auto& c : CATEGORIES) { IncidentCategory x{c.id, c.parent, c.name, c.code, c.ts, c.caps}; repos.categories->insert(x); }
    if (repos.resources->findTypes().empty())
        for (const auto& t : TYPES) { ResourceType x{t.id, t.name, t.category, t.caps}; repos.resources->insertType(x); }

    if (!repos.incidents->findAll().empty() || !repos.resources->findAll().empty() || !repos.locations->findLocations().empty()) {
        bootstrap();
        if (auth.userCount() == 0) auth.createUser("admin", "Administrator", "Nexa@2026", Role::Admin, actor);
        return false;
    }

    // ---- locations & roads ----
    DynamicArray<long> placeIds;
    for (const auto& p : PLACES) { Location l; l.name = p.name; l.kind = p.kind; l.lat = p.lat; l.lon = p.lon; repos.locations->insertLocation(l); placeIds.pushBack(l.id); }
    int ri = 0;
    for (const auto& r : ROADS) {
        const Place& a = PLACES[r.a]; const Place& b = PLACES[r.b];
        double straight = geo::haversineKm(a.lat, a.lon, b.lat, b.lon);
        double factor = r.cls == 1 ? 1.15 : 1.3;           // road length vs straight line (simulated)
        double speed = r.cls == 1 ? 55.0 : (r.cls == 2 ? 18.0 : 28.0);
        Road x; x.fromId = placeIds[r.a]; x.toId = placeIds[r.b];
        x.name = std::string(a.name) + " – " + b.name;
        x.distanceKm = std::round(straight * factor * 100) / 100;
        x.travelMinutes = std::round(x.distanceKm / speed * 60 * 10) / 10;
        x.penalty = r.cls == 2 ? 2.0 : (r.cls == 1 ? 0.5 : 0.0);   // cost mode: tolls/narrow lanes (simulated)
        x.blocked = ri == BLOCKED_ROAD_INDEX;
        repos.locations->insertRoad(x);
        ++ri;
    }
    // hospitals become graph vertices too, linked to their 2 nearest junctions
    auto linkToNearest = [&](long locId, double lat, double lon, const char* name) {
        int best[2] = {-1, -1}; double bd[2] = {1e18, 1e18};
        for (int i = 0; i < (int)(sizeof(PLACES) / sizeof(PLACES[0])); ++i) {
            if (std::string(PLACES[i].kind) != "junction") continue;
            double d = geo::haversineKm(lat, lon, PLACES[i].lat, PLACES[i].lon);
            if (d < bd[0]) { bd[1] = bd[0]; best[1] = best[0]; bd[0] = d; best[0] = i; }
            else if (d < bd[1]) { bd[1] = d; best[1] = i; }
        }
        for (int k = 0; k < 2; ++k) {
            if (best[k] < 0) continue;
            Road x; x.fromId = locId; x.toId = placeIds[best[k]]; x.name = std::string(name) + " access";
            x.distanceKm = std::round(bd[k] * 1.3 * 100) / 100 + 0.05;
            x.travelMinutes = std::round(x.distanceKm / 22.0 * 60 * 10) / 10;
            repos.locations->insertRoad(x);
        }
    };
    for (const auto& h : HOSPITALS) {
        Location l; l.name = h.name; l.kind = "hospital"; l.lat = h.lat; l.lon = h.lon;
        repos.locations->insertLocation(l);
        linkToNearest(l.id, h.lat, h.lon, h.name);
    }

    bootstrap();     // graph + taxonomy + types are now available to the services

    // ---- hospitals ----
    for (const auto& h : HOSPITALS) {
        HospitalInput in; in.name = h.name; in.category = h.cat; in.lat = h.lat; in.lon = h.lon;
        in.totalBeds = h.beds; in.availableBeds = h.avail; in.icuBeds = h.icu; in.icuAvailable = h.icuAvail;
        in.capabilities = h.caps; in.contact = "SIMULATED"; in.address = "Simulated capacity — demo data";
        hospitals.create(in, actor);
    }

    // ---- resources at stations (small deterministic offsets around the station) ----
    int serial = 100;
    for (const auto& st : STATIONS) {
        const Place& p = PLACES[st.place];
        for (int u = 0; u < st.n; ++u) {
            ++serial;
            const Type& t = TYPES[st.units[u].type - 1];
            ResourceInput in;
            in.code = std::string(typePrefix(t.id)) + "-" + st.prefix + std::to_string(serial);
            in.name = std::string(t.name) + " (" + st.prefix + ")";
            in.typeId = t.id; in.capacity = st.units[u].capacity; in.available = st.units[u].capacity;
            in.lat = p.lat + 0.0012 * std::cos(serial * 1.7); in.lon = p.lon + 0.0012 * std::sin(serial * 1.7);
            in.capabilities = t.caps; in.organization = st.org;
            resources.create(in, actor);
        }
    }
    // one unit in maintenance (shows hard filtering)
    if (!store.resources.items().empty()) resources.setStatus(store.resources.items()[2].id, ResourceStatus::Maintenance, actor);

    // ---- incidents ----
    long now = util::nowEpoch();
    DynamicArray<long> incIds;
    for (const auto& d : INCIDENTS) {
        const Place& p = PLACES[d.place];
        IncidentInput in;
        in.title = d.title; in.description = d.desc; in.categoryId = d.cat; in.severity = d.sev;
        in.lat = p.lat + d.dLat; in.lon = p.lon + d.dLon; in.locationName = p.name;
        in.affectedPeople = d.affected; in.requiredCapabilities = d.caps; in.requiredUnits = d.units;
        in.reportedBy = "control-room"; in.reporterContact = "112 (simulated)";
        in.createdAt = now - (long)(d.hoursAgo * 3600);
        in.simulated = true;
        incIds.pushBack(incidents.create(in, actor).id);
    }

    // ---- users ----
    if (auth.userCount() == 0) {
        auth.createUser("admin", "Administrator", "Nexa@2026", Role::Admin, actor);
        auth.createUser("operator", "Incident Operator", "Operator@2026", Role::IncidentOperator, actor);
        auth.createUser("coordinator", "Resource Coordinator", "Coord@2026", Role::ResourceCoordinator, actor);
        auth.createUser("medic", "Medical Coordinator", "Medic@2026", Role::MedicalCoordinator, actor);
        auth.createUser("viewer", "Read-only Viewer", "Viewer@2026", Role::Viewer, actor);
    }

    // ---- operational history through the real workflow ----
    auto tryAllocate = [&](long incId) { try { AllocationRequest r; r.incidentId = incId; allocation.allocate(r, "operator"); } catch (const NexaError& e) { util::Logger::instance().warn("seeder", e.what()); } };
    tryAllocate(incIds[0]);   // building collapse
    tryAllocate(incIds[1]);   // chemical fire
    tryAllocate(incIds[2]);   // flood
    tryAllocate(incIds[7]);   // heat-stroke
    // resolved example: allocate then complete & resolve
    tryAllocate(incIds[11]);
    for (const auto& dpl : allocation.list("ACTIVE")) if (dpl.incidentId == incIds[11]) allocation.complete(dpl.id, "operator");
    try { incidents.changeStatus(incIds[11], IncidentStatus::Resolved, "operator", "roads cleared"); } catch (const NexaError&) {}
    // a hospital reservation
    try { hospitals.reserve(store.hospitals.items()[0].id, incIds[0], 6, 12, "medic"); } catch (const NexaError&) {}

    // ---- emergency requests (mix of FIFO and escalated) ----
    struct ReqDef { int inc; const char* type; int urg; long people; const char* desc; };
    const ReqDef REQS[] = {
        {3, "medical", 5, 30, "Bus passengers injured, need trauma ambulances"},
        {4, "evacuation", 4, 250, "Evacuate residential block downwind of the leak"},
        {5, "fire", 3, 40, "Second fire tender needed at market lane"},
        {6, "medical", 4, 60, "Injured commuters at the foot-over bridge"},
        {8, "supplies", 2, 400, "Drinking water for relief camp"},
        {9, "medical", 2, 80, "Medical team for gastroenteritis cases"},
        {10, "rescue", 5, 2, "Two people missing under the collapsed wall"},
        {13, "rescue", 3, 35, "Passengers stranded in flooded bus"},
    };
    for (const auto& r : REQS) {
        RequestInput in; in.incidentId = incIds[r.inc]; in.assistanceType = r.type; in.urgency = r.urg; in.peopleAffected = r.people;
        in.description = r.desc; in.requestedBy = "field-unit";
        try { requests.create(in, "operator"); } catch (const NexaError& e) { util::Logger::instance().warn("seeder", e.what()); }
    }
    util::Logger::instance().info("seeder", "demo dataset created (SIMULATED)");
    return true;
}

} // namespace nexa::svc
