#pragma once
// ============================================================================
// NEXA :: controller helpers — authentication, JSON body/query parsing,
// uniform error mapping (NexaError -> HTTP status) and response building.
// Error body: { "error": { "code": "VALIDATION_ERROR", "message": "..." } }
// ============================================================================
#include <drogon/HttpController.h>
#include <drogon/HttpResponse.h>
#include <functional>
#include <string>
#include "services/NexaCore.hpp"
#include "utilities/Errors.hpp"

namespace nexa::api {

using drogon::HttpRequestPtr;
using drogon::HttpResponsePtr;
using Callback = std::function<void(const HttpResponsePtr&)>;

/// Set once in main() after the core has been constructed and bootstrapped.
void setCore(svc::NexaCore* core);
svc::NexaCore* corePtr();
svc::NexaCore& core();

HttpResponsePtr jsonResponse(const Json::Value& v, int status = 200);
HttpResponsePtr errorResponse(int status, const std::string& code, const std::string& message);

std::string bearerToken(const HttpRequestPtr& req);
svc::Session authorize(const HttpRequestPtr& req, svc::Permission p);
std::string actorOf(const svc::Session& s);

// ---- body ----
Json::Value body(const HttpRequestPtr& req);
std::string sv(const Json::Value& j, const char* key, const std::string& def = "");
long lv(const Json::Value& j, const char* key, long def = 0);
int iv(const Json::Value& j, const char* key, int def = 0);
double dv(const Json::Value& j, const char* key, double def = 0);
bool bv(const Json::Value& j, const char* key, bool def = false);
ds::DynamicArray<long> longs(const Json::Value& j, const char* key);

// ---- query string ----
std::string qs(const HttpRequestPtr& req, const char* key, const std::string& def = "");
long ql(const HttpRequestPtr& req, const char* key, long def = 0);
int qi(const HttpRequestPtr& req, const char* key, int def = 0);
double qd(const HttpRequestPtr& req, const char* key, double def = 0);
bool qb(const HttpRequestPtr& req, const char* key, bool def = false);

/// Runs `fn(session)` with authorisation + error mapping; fn returns Json::Value.
void run(const HttpRequestPtr& req, Callback& cb, svc::Permission p, const std::function<Json::Value(const svc::Session&)>& fn, int okStatus = 200);
/// Same, without authentication (health, login).
void runPublic(const HttpRequestPtr& req, Callback& cb, const std::function<Json::Value()>& fn, int okStatus = 200);
/// Same as run() but the handler builds the full response (CSV downloads).
void runRaw(const HttpRequestPtr& req, Callback& cb, svc::Permission p, const std::function<HttpResponsePtr(const svc::Session&)>& fn);

} // namespace nexa::api
