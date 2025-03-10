#include <mutex>

#include "ICommand.h"
#include "IQuery.h"
#include "ITransaction.h"
#include "sqlite3ppext.h"

namespace HomeCompa::DB::Impl::Sqlite
{

std::unique_ptr<ICommand> CreateCommandImpl(sqlite3pp::database& db, std::string_view command);
std::unique_ptr<IQuery> CreateQueryImpl(std::mutex& mutex, sqlite3pp::database& db, std::string_view query);

namespace
{

class Transaction : virtual public DB::ITransaction
{
public:
	explicit Transaction(std::mutex& mutex, sqlite3pp::database& db)
		: m_lock(mutex)
		, m_db(db)
		, m_transaction(db)
	{
	}

	~Transaction() override
	{
		if (m_active)
			m_transaction.rollback();
	}

private: // Transaction
	bool Commit() override
	{
		m_active = false;
		return m_transaction.commit() == 0;
	}

	bool Rollback() override
	{
		m_active = false;
		return m_transaction.rollback() == 0;
	}

	std::unique_ptr<ICommand> CreateCommand(std::string_view command) override
	{
		return CreateCommandImpl(m_db, command);
	}

	std::unique_ptr<IQuery> CreateQuery(std::string_view query) override
	{
		return CreateQueryImpl(m_queryMutex, m_db, query);
	}

private:
	std::lock_guard<std::mutex> m_lock;
	sqlite3pp::database& m_db;
	sqlite3pp::transaction m_transaction;
	bool m_active { true };
	std::mutex m_queryMutex;
};

} // namespace

std::unique_ptr<DB::ITransaction> CreateTransactionImpl(std::mutex& mutex, sqlite3pp::database& db)
{
	return std::make_unique<Transaction>(mutex, db);
}

} // namespace HomeCompa::DB::Impl::Sqlite
