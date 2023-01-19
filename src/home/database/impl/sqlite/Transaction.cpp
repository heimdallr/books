#include <shared_mutex>

#include "sqlite3ppext.h"

#include "database/interface/Command.h"
#include "database/interface/Query.h"
#include "database/interface/Transaction.h"

namespace HomeCompa::DB::Impl::Sqlite {

std::unique_ptr<Command> CreateCommandImpl(sqlite3pp::database & db, const std::string_view & command);
std::unique_ptr<Query> CreateQueryImpl(std::shared_mutex & mutex, sqlite3pp::database & db, const std::string_view & query);

namespace {

class Transaction
	: virtual public DB::Transaction
{
public:
	explicit Transaction(std::shared_mutex & mutex, sqlite3pp::database & db)
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
	void Commit() override
	{
		m_active = false;
		m_transaction.commit();
	}

	void Rollback() override
	{
		m_active = false;
		m_transaction.rollback();
	}

	std::unique_ptr<Command> CreateCommand(const std::string_view & command) override
	{
		return CreateCommandImpl(m_db, command);
	}

	std::unique_ptr<Query> CreateQuery(const std::string_view & query) override
	{
		return CreateQueryImpl(m_queryMutex, m_db, query);
	}

private:
	std::unique_lock<std::shared_mutex> m_lock;
	sqlite3pp::database & m_db;
	sqlite3pp::transaction m_transaction;
	bool m_active { true };
	std::shared_mutex m_queryMutex;
};

}

std::unique_ptr<DB::Transaction> CreateTransactionImpl(std::shared_mutex & mutex, sqlite3pp::database & db)
{
	return std::make_unique<Transaction>(mutex, db);
}

}
