#include <cassert>
#include <mutex>

#include "sqlite3ppext.h"

#include "Database.h"
#include "IQuery.h"

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
	Query(std::mutex & mutex, sqlite3pp::database & db, const std::string_view query)
		: m_lock(mutex)
		, m_query(db, LogStatement(query).data())
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

	std::string ColumnName(const size_t index) const override
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

	int Bind(const size_t index) override
	{
		return m_query.bind(Index(index) + 1);
	}

	int BindInt(const size_t index, const int value) override
	{
		return m_query.bind(Index(index) + 1, value);
	}

	int BindLong(const size_t index, const long long int value) override
	{
		return m_query.bind(Index(index) + 1, value);
	}

	int BindDouble(const size_t index, const double value) override
	{
		return m_query.bind(Index(index) + 1, value);
	}

	int BindString(const size_t index, const std::string_view value) override
	{
		return m_query.bind(Index(index) + 1, value, sqlite3pp::copy);
	}

	int Bind(const std::string_view name) override
	{
		return m_query.bind(name.data());
	}

	int BindInt(const std::string_view name, const int value) override
	{
		return m_query.bind(name.data(), value);
	}

	int BindLong(const std::string_view name, const long long int value) override
	{
		return m_query.bind(name.data(), value);
	}

	int BindDouble(const std::string_view name, const double value) override
	{
		return m_query.bind(name.data(), value);
	}

	int BindString(const std::string_view name, const std::string_view value) override
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
