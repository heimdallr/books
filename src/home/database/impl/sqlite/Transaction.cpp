#include <shared_mutex>

#include "database/interface/Transaction.h"

namespace HomeCompa::DB::Impl::Sqlite {

namespace {

class Transaction
	: virtual public DB::Transaction
{
public:
	explicit Transaction(std::shared_mutex & mutex)
		: m_lock(mutex)
	{
	}

private: // Transaction

private:
	std::unique_lock<std::shared_mutex> m_lock;
};

}

std::unique_ptr<DB::Transaction> CreateTransactionImpl(std::shared_mutex & mutex)
{
	return std::make_unique<Transaction>(mutex);
}

}
