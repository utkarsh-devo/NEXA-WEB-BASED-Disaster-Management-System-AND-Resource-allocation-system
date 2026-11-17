#pragma once
// ============================================================================
// NEXA :: shared service context, live-event sink and audit trail.
// ============================================================================
#include <mutex>
#include <string>
#include "EngineConfig.hpp"
#include "Store.hpp"
#include "../repositories/Repositories.hpp"
#include "../utilities/Logger.hpp"
#include "../utilities/Strings.hpp"

namespace nexa::svc {

/// A live operational update pushed to connected browsers (WebSocket).
struct LiveEvent {
    std::string type;        // incident.created | incident.updated | priority.changed | resource.updated
                             // deployment.created | deployment.closed | request.created | simulation.step
    long entityId = 0;
    std::string title;
    std::string detail;
    double lat = 0, lon = 0;
    int severity = 0;
};

struct IEventSink {
    virtual ~IEventSink() = default;
    virtual void publish(const LiveEvent& e) = 0;
};

struct NullSink : IEventSink { void publish(const LiveEvent&) override {} };

/// Records the latest events in memory too (tests / dashboard "recent activity").
struct RecordingSink : IEventSink {
    std::mutex m;
    ds::DynamicArray<LiveEvent> events;
    void publish(const LiveEvent& e) override { std::lock_guard<std::mutex> g(m); events.pushBack(e); }
};

struct ServiceContext {
    EngineConfig& config;
    repo::RepositoryBundle& repos;
    OperationalStore& store;
    IEventSink& sink;
};

/// Writes to the TXT log and to the system_logs table.
class AuditService {
public:
    explicit AuditService(ServiceContext& c) : ctx_(c) {}
    void record(const std::string& level, const std::string& source, const std::string& message, const std::string& actor = "system") {
        util::Logger::instance().log(level, source, message, actor);
        try {
            model::SystemLog l; l.level = level; l.source = source; l.message = message; l.actor = actor; l.createdAt = util::nowEpoch();
            ctx_.repos.logs->insert(l);
        } catch (...) { util::Logger::instance().error("audit", "failed to persist system log"); }
    }
    ds::DynamicArray<model::SystemLog> recent(int limit) { return ctx_.repos.logs->recent(limit); }
private:
    ServiceContext& ctx_;
};

} // namespace nexa::svc
