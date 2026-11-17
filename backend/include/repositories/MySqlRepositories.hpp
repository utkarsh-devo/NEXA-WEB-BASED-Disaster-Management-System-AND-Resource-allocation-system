#pragma once
// ============================================================================
// NEXA :: MySQL repositories (Drogon ORM DbClient, synchronous calls).
// All SQL lives here — controllers and data structures never touch SQL
// (spec §6). Multi-record writes run inside a DbClient transaction.
// Timestamps: DATETIME columns, converted with FROM_UNIXTIME / UNIX_TIMESTAMP
// in the same session, so round-trips are exact.
// ============================================================================
#include <drogon/orm/DbClient.h>
#include "Repositories.hpp"

namespace nexa::repo {

/// Builds the full repository bundle over one connection pool.
RepositoryBundle makeMySqlBundle(const drogon::orm::DbClientPtr& db);

} // namespace nexa::repo
