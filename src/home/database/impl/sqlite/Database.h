#pragma once

#include <memory>
#include <string>

#include "log.h"

#include "export/dbsqlite.h"

namespace HomeCompa::DB
{
class IDatabase;
}

namespace HomeCompa::DB::Impl::Sqlite
{

DBSQLITE_EXPORT std::unique_ptr<IDatabase> CreateDatabase(const std::string& connection);

inline std::string_view LogStatement(const std::string_view statement)
{
#ifndef NDEBUG
	PLOGD << statement;
#endif
	return statement;
}

}
