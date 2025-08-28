#pragma once

#include <memory>
#include <string>

#include "export/dbfactory.h"

namespace HomeCompa::DB
{
class IDatabase;
}

namespace HomeCompa::DB::Factory
{

enum class Impl
{
	Sqlite,
};

DBFACTORY_EXPORT std::unique_ptr<IDatabase> Create(Impl impl, const std::string& connection);

}
