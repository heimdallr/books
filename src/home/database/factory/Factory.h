#pragma once

#include <memory>
#include <string>

#include "export/DatabaseFactoryLib.h"

namespace HomeCompa::DB {
class Database;
}

namespace HomeCompa::DB::Factory {

enum class Impl
{
	Sqlite,
};

DATABASEFACTORY_API std::unique_ptr<Database> Create(Impl impl, const std::string & connection);
	
}
