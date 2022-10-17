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

class Database
	: virtual public DB::Database
{
public:
	explicit Database(const std::string & connection)
		: m_db(connection.data(), SQLITE_OPEN_READONLY)
	{
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
	std::shared_mutex m_guard;
	sqlite3pp::database m_db;
};

}

std::unique_ptr<DB::Database> CreateDatabase(const std::string & connection)
{
	return std::make_unique<Impl::Sqlite::Database>(connection);
}

}
