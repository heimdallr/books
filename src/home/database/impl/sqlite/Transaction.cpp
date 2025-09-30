#include <mutex>

#include "fnd/NonCopyMovable.h"

#include "ICommand.h"
#include "IQuery.h"
#include "ITransaction.h"
#include "sqlite3ppext.h"

namespace HomeCompa::DB::Impl::Sqlite
{

std::unique_ptr<ICommand> CreateCommandImpl(sqlite3pp::database& db, std::string_view command);
std::unique_ptr<IQuery>   CreateQueryImpl(std::mutex& mutex, sqlite3pp::database& db, std::string_view query);

namespace
{

class Transaction final : virtual public ITransaction
{
	NON_COPY_MOVABLE(Transaction)

public:
	Transaction(std::mutex& mutex, sqlite3pp::database& db)
		: m_lock(std::make_unique<std::lock_guard<std::mutex>>(mutex))
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
		m_active          = false;
		const auto result = m_transaction.commit() == 0;
		m_lock.reset();
		return result;
	}

	bool Rollback() override
	{
		m_active          = false;
		const auto result = m_transaction.rollback() == 0;
		m_lock.reset();
		return result;
	}

	std::unique_ptr<ICommand> CreateCommand(const std::string_view command) override
	{
		return CreateCommandImpl(m_db, command);
	}

	std::unique_ptr<IQuery> CreateQuery(const std::string_view query) override
	{
		return CreateQueryImpl(m_queryMutex, m_db, query);
	}

private:
	std::unique_ptr<std::lock_guard<std::mutex>> m_lock;
	sqlite3pp::database&                         m_db;
	sqlite3pp::transaction                       m_transaction;
	bool                                         m_active { true };
	std::mutex                                   m_queryMutex;
};

} // namespace

std::unique_ptr<ITransaction> CreateTransactionImpl(std::mutex& mutex, sqlite3pp::database& db)
{
	return std::make_unique<Transaction>(mutex, db);
}

} // namespace HomeCompa::DB::Impl::Sqlite
