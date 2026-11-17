#pragma once
// ============================================================================
// NEXA :: authentication, users, dashboard, settings and system endpoints
// ============================================================================
#include <drogon/HttpController.h>

namespace nexa::api {

using drogon::HttpRequestPtr;
using drogon::HttpResponsePtr;
using Cb = std::function<void(const HttpResponsePtr&)>;

class AuthController : public drogon::HttpController<AuthController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AuthController::login, "/api/auth/login", drogon::Post);
    ADD_METHOD_TO(AuthController::logout, "/api/auth/logout", drogon::Post);
    ADD_METHOD_TO(AuthController::me, "/api/auth/me", drogon::Get);
    METHOD_LIST_END
    void login(const HttpRequestPtr& req, Cb&& cb);
    void logout(const HttpRequestPtr& req, Cb&& cb);
    void me(const HttpRequestPtr& req, Cb&& cb);
};

class UserController : public drogon::HttpController<UserController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(UserController::list, "/api/users", drogon::Get);
    ADD_METHOD_TO(UserController::create, "/api/users", drogon::Post);
    ADD_METHOD_TO(UserController::update, "/api/users/{1}", drogon::Put);
    ADD_METHOD_TO(UserController::resetPassword, "/api/users/{1}/password", drogon::Post);
    METHOD_LIST_END
    void list(const HttpRequestPtr& req, Cb&& cb);
    void create(const HttpRequestPtr& req, Cb&& cb);
    void update(const HttpRequestPtr& req, Cb&& cb, long id);
    void resetPassword(const HttpRequestPtr& req, Cb&& cb, long id);
};

class SystemController : public drogon::HttpController<SystemController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(SystemController::health, "/api/health", drogon::Get);
    ADD_METHOD_TO(SystemController::dashboard, "/api/dashboard", drogon::Get);
    ADD_METHOD_TO(SystemController::activity, "/api/activity", drogon::Get);
    ADD_METHOD_TO(SystemController::logs, "/api/system/logs", drogon::Get);
    ADD_METHOD_TO(SystemController::reload, "/api/system/reload", drogon::Post);
    ADD_METHOD_TO(SystemController::getWeights, "/api/settings/weights", drogon::Get);
    ADD_METHOD_TO(SystemController::putWeights, "/api/settings/weights", drogon::Put);
    ADD_METHOD_TO(SystemController::undo, "/api/actions/undo", drogon::Post);
    METHOD_LIST_END
    void health(const HttpRequestPtr& req, Cb&& cb);
    void dashboard(const HttpRequestPtr& req, Cb&& cb);
    void activity(const HttpRequestPtr& req, Cb&& cb);
    void logs(const HttpRequestPtr& req, Cb&& cb);
    void reload(const HttpRequestPtr& req, Cb&& cb);
    void getWeights(const HttpRequestPtr& req, Cb&& cb);
    void putWeights(const HttpRequestPtr& req, Cb&& cb);
    void undo(const HttpRequestPtr& req, Cb&& cb);
};

} // namespace nexa::api
