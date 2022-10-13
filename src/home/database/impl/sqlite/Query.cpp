#include <cassert>
#include <shared_mutex>

#include "sqlite3ppext.h"

#include "database/interface/Query.h"

namespace HomeCompa::DB::Impl::Sqlite {

namespace {

class Query
	: virtual public DB::Query
{
public:
	Query(std::shared_mutex & mutex, sqlite3pp::database & db, const std::string & query)
		: m_lock(mutex)
		, m_query(db, query.data())
		, m_it(m_query.begin())
	{
	}

private: // Query
	bool Eof() override
	{
		return m_it == m_query.end();
	}
	
	void Next() override
	{
		assert(!Eof());
		++m_it;
	}

	size_t ColumnCount() const override
	{
		assert(m_query.column_count() >= 0);
		return static_cast<size_t>(m_query.column_count());
	}

	int GetInt(size_t index) const override
	{
		return Get<int>(index);
	}

	long long int GetLong(size_t index) const override
	{
		return Get<long long int>(index);
	}
	
	double GetDouble(size_t index) const override
	{
		return Get<double>(index);
	}

	std::string GetString(size_t index) const override
	{
		return Get<std::string>(index);
	}

	template<typename T>
	T Get(const size_t index) const
	{
		return (*m_it).get<T>(static_cast<int>(index));
	}

private:
	std::shared_lock<std::shared_mutex> m_lock;
	sqlite3pp::query m_query;
	sqlite3pp::query::iterator m_it;
};

}

std::unique_ptr<DB::Query> CreateQueryImpl(std::shared_mutex & mutex, sqlite3pp::database & db, const std::string & query)
{
	return std::make_unique<Query>(mutex, db, query);
}

}
