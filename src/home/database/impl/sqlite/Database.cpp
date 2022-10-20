#include <shared_mutex>
#include <string>

#include "database/interface/Database.h"
#include "database/interface/Transaction.h"
#include "database/interface/Query.h"

#include "sqlite3ppext.h"

#include "Database.h"

namespace HomeCompa::DB::Impl::Sqlite {

std::unique_ptr<Transaction> CreateTransactionImpl(std::shared_mutex &);
std::unique_ptr<Query> CreateQueryImpl(std::shared_mutex & mutex, sqlite3pp::database & db, const std::string & query);

namespace {

constexpr auto PATH = "path";
constexpr auto EXTENSION = "extension";

using ConnectionParameters = std::multimap<std::string, std::string>;

std::vector<std::string> Split(const std::string & src, const char separator)
{
	std::vector<std::string> result;
	std::istringstream stream(src);
	std::string s;
	while (std::getline(stream, s, separator))
		result.push_back(s);

	return result;
}

ConnectionParameters ParseConnectionString(const std::string & connection)
{
	ConnectionParameters result;
	for (const auto & parameter : Split(connection, ';'))
	{
		auto pair = Split(parameter, '=');
		if (std::size(pair) == 1)
			result.emplace(PATH, std::move(pair.front()));
		else
			result.emplace(std::move(pair.front()), std::move(pair.back()));
	}

	return result;
}

const std::string & GetValue(const ConnectionParameters & parameters, const std::string & key)
{
	auto [begin, end] = parameters.equal_range(key);
	if (begin == end)
		throw std::invalid_argument("must pass path");

	return begin->second;
}

class Database
	: virtual public DB::Database
{
public:
	explicit Database(const std::string & connection)
		: m_connectionParameters(ParseConnectionString(connection))
		, m_db(GetValue(m_connectionParameters, PATH).data(), SQLITE_OPEN_READONLY)
	{
		for (auto [begin, end] = m_connectionParameters.equal_range(EXTENSION); begin != end; ++begin)
			m_db.load_extension(begin->second.data());
	}

private: // Database
	[[nodiscard]] std::unique_ptr<Transaction> CreateTransaction() override
	{
		return CreateTransactionImpl(m_guard);
	}

	[[nodiscard]] std::unique_ptr<Query> CreateQuery(const std::string & query) override
	{
		return CreateQueryImpl(m_guard, m_db, query);
	}

private:
	ConnectionParameters m_connectionParameters;
	std::shared_mutex m_guard;
	sqlite3pp::database m_db;
};

}

std::unique_ptr<DB::Database> CreateDatabase(const std::string & connection)
{
	return std::make_unique<Database>(connection);
}

}
