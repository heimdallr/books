#include <shared_mutex>

#include "sqlite3ppext.h"

#include "database/interface/Transaction.h"
#include "database/interface/Command.h"

namespace HomeCompa::DB::Impl::Sqlite {

std::unique_ptr<Command> CreateCommandImpl(sqlite3pp::database & db, const std::string & command);

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

	std::unique_ptr<Command> CreateCommand(const std::string & command) override
	{
		return CreateCommandImpl(m_db, command);
	}

private:
	std::unique_lock<std::shared_mutex> m_lock;
	sqlite3pp::database & m_db;
	sqlite3pp::transaction m_transaction;
	bool m_active { true };
};

}

std::unique_ptr<DB::Transaction> CreateTransactionImpl(std::shared_mutex & mutex, sqlite3pp::database & db)
{
	return std::make_unique<Transaction>(mutex, db);
}

}
