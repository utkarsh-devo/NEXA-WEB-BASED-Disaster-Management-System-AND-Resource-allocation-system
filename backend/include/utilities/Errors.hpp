#pragma once
// ============================================================================
// NEXA :: typed errors. Services throw these; controllers translate them to
// HTTP status codes in one place (controllers/ControllerUtils).
// ============================================================================
#include <stdexcept>
#include <string>

namespace nexa {

class NexaError : public std::runtime_error {
public:
    NexaError(int httpStatus, std::string code, const std::string& msg)
        : std::runtime_error(msg), status_(httpStatus), code_(std::move(code)) {}
    int status() const { return status_; }
    const std::string& code() const { return code_; }
private:
    int status_;
    std::string code_;
};

struct ValidationError : NexaError { explicit ValidationError(const std::string& m) : NexaError(400, "VALIDATION_ERROR", m) {} };
struct NotFoundError   : NexaError { explicit NotFoundError(const std::string& m)   : NexaError(404, "NOT_FOUND", m) {} };
struct ConflictError   : NexaError { explicit ConflictError(const std::string& m)   : NexaError(409, "CONFLICT", m) {} };
struct AuthError       : NexaError { explicit AuthError(const std::string& m)       : NexaError(401, "UNAUTHORIZED", m) {} };
struct ForbiddenError  : NexaError { explicit ForbiddenError(const std::string& m)  : NexaError(403, "FORBIDDEN", m) {} };
struct PersistenceError: NexaError { explicit PersistenceError(const std::string& m): NexaError(500, "PERSISTENCE_ERROR", m) {} };

} // namespace nexa
