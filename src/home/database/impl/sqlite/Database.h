#pragma once

#include <memory>
#include <string>

#include "databasesqlite_export.h"

namespace HomeCompa::DB {
class IDatabase;
}

namespace HomeCompa::DB::Impl::Sqlite {

DATABASESQLITE_EXPORT std::unique_ptr<IDatabase> CreateDatabase(const std::string & connection);

}
