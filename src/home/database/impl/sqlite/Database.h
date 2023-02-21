#pragma once

#include <memory>
#include <string>

#include "export/DatabaseSqliteLib.h"

namespace HomeCompa::DB {
class IDatabase;
}

namespace HomeCompa::DB::Impl::Sqlite {

DATABASESQLITE_API std::unique_ptr<IDatabase> CreateDatabase(const std::string & connection);

}
