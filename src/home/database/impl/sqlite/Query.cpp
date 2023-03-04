#include <cassert>
#include <mutex>

#include "sqlite3ppext.h"

#include "database/interface/IQuery.h"

namespace HomeCompa::DB::Impl::Sqlite {

namespace {

int Index(const size_t index)
{
	return static_cast<int>(index);
}

class Query
	: virtual public DB::IQuery
{
public:
	Query(std::mutex & mutex, sqlite3pp::database & db, std::string_view query)
		: m_lock(mutex)
		, m_query(db, query.data())
	{
	}

private: // Query
	void Execute() override
	{
		m_it = m_query.begin();
	}

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

	std::string ColumnName(size_t index) const override
	{
		return m_query.column_name(Index(index));
	}

	int GetInt(const size_t index) const override
	{
		return Get<int>(index);
	}

	long long int GetLong(const size_t index) const override
	{
		return Get<long long int>(index);
	}

	double GetDouble(const size_t index) const override
	{
		return Get<double>(index);
	}

	std::string GetString(const size_t index) const override
	{
		return Get<std::string>(index);
	}

	const char * GetRawString(const size_t index) const override
	{
		return Get<const char *>(index);
	}

	int BindInt(size_t index, int value) override
	{
		return m_query.bind(Index(index) + 1, value);
	}

	int BindLong(size_t index, long long int value) override
	{
		return m_query.bind(Index(index) + 1, value);
	}

	int BindDouble(size_t index, double value) override
	{
		return m_query.bind(Index(index) + 1, value);
	}

	int BindString(size_t index, const std::string & value) override
	{
		return m_query.bind(Index(index) + 1, value, sqlite3pp::copy);
	}

	int BindInt(std::string_view name, int value) override
	{
		return m_query.bind(name.data(), value);
	}

	int BindLong(std::string_view name, long long int value) override
	{
		return m_query.bind(name.data(), value);
	}

	int BindDouble(std::string_view name, double value) override
	{
		return m_query.bind(name.data(), value);
	}

	int BindString(std::string_view name, const std::string & value) override
	{
		return m_query.bind(name.data(), value, sqlite3pp::copy);
	}

private:
	template<typename T>
	T Get(const size_t index) const
	{
		return (*m_it).get<T>(Index(index));
	}

private:
	std::lock_guard<std::mutex> m_lock;
	sqlite3pp::query m_query;
	sqlite3pp::query::iterator m_it;
};

}

std::unique_ptr<DB::IQuery> CreateQueryImpl(std::mutex & mutex, sqlite3pp::database & db, std::string_view query)
{
	return std::make_unique<Query>(mutex, db, query);
}

}
