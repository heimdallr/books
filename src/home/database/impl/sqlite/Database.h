#pragma once

#include "export/DatabaseSqliteLib.h"

namespace HomeCompa::DB {
class Database;
}

namespace HomeCompa::DB::Impl::Sqlite {

DATABASESQLITE_API std::unique_ptr<Database> CreateDatabase(const std::string & connection);

}
