#pragma once

#include <memory>
#include <string>

#include "log.h"

#include "export/databasesqlite.h"

namespace HomeCompa::DB
{
class IDatabase;
}

namespace HomeCompa::DB::Impl::Sqlite
{

DATABASESQLITE_EXPORT std::unique_ptr<IDatabase> CreateDatabase(const std::string& connection);

inline std::string_view LogStatement(std::string_view statement)
{
#ifndef NDEBUG
	PLOGD << statement;
#endif
	return statement;
}

}
