#include "Factory.h"

#include <cassert>

#include "database/interface/IDatabase.h"

#include "database/impl/sqlite/Database.h"

namespace HomeCompa::DB::Factory
{

namespace
{

using FactoryCreator = std::unique_ptr<IDatabase> (*)(const std::string& connection);
constexpr FactoryCreator g_creators[] {
	&DB::Impl::Sqlite::CreateDatabase,
};

size_t ToIndex(const Impl impl)
{
	const auto index = static_cast<size_t>(impl);
	assert(index < std::size(g_creators));
	return index;
}

auto GetCreator(const Impl impl)
{
	return g_creators[ToIndex(impl)];
}

}

std::unique_ptr<IDatabase> Create(const Impl impl, const std::string& connection)
{
	const auto creator = GetCreator(impl);
	assert(creator);
	return creator(connection);
}

} // namespace HomeCompa::DB::Factory
