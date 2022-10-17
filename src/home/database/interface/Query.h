#pragma once

namespace HomeCompa::DB {

class Query;
template<typename T> T GetImpl(const Query & query, size_t index) = delete;
template<typename T> int BindImpl(Query & query, size_t index, const T & value) = delete;
template<typename T> int BindImpl(Query & query, std::string_view name, const T & value) = delete;

class Query
{
public:
	virtual ~Query() = default;

public:
	virtual void Execute() = 0;
	virtual bool Eof() = 0;
	virtual void Next() = 0;
	virtual size_t ColumnCount() const = 0;

	virtual int GetInt(size_t index) const = 0;
	virtual long long int GetLong(size_t index) const = 0;
	virtual double GetDouble(size_t index) const = 0;
	virtual std::string GetString(size_t index) const = 0;
	virtual const char * GetRawString(size_t index) const = 0;

	template<typename T>
	T Get(size_t index) const
	{
		return GetImpl<T>(*this, index);
	}

	virtual int BindInt(size_t index, int value) = 0;
	virtual int BindLong(size_t index, long long int value) = 0;
	virtual int BindDouble(size_t index, double value) = 0;
	virtual int BindString(size_t index, const std::string & value) = 0;

	template<typename T>
	int Bind(size_t index, const T & value)
	{
		return BindImpl<T>(*this, index, value);
	}

	virtual int BindInt(std::string_view name, int value) = 0;
	virtual int BindLong(std::string_view name, long long int value) = 0;
	virtual int BindDouble(std::string_view name, double value) = 0;
	virtual int BindString(std::string_view name, const std::string & value) = 0;

	template<typename T>
	int Bind(std::string_view name, const T & value)
	{
		return BindImpl<T>(*this, name, value);
	}
};

template<> inline int GetImpl(const Query & query, size_t index)
{
	return query.GetInt(index);
}

template<> inline long long int GetImpl(const Query & query, size_t index)
{
	return query.GetLong(index);
}

template<> inline double GetImpl(const Query & query, size_t index)
{
	return query.GetDouble(index);
}

template<> inline std::string GetImpl(const Query & query, size_t index)
{
	return query.GetString(index);
}

template<> inline const char * GetImpl(const Query & query, size_t index)
{
	return query.GetRawString(index);
}

template<> inline int BindImpl(Query & query, size_t index, const int & value)
{
	return query.BindInt(index, value);
}

template<> inline int BindImpl(Query & query, size_t index, const long long int & value)
{
	return query.BindLong(index, value);
}

template<> inline int BindImpl(Query & query, size_t index, const double & value)
{
	return query.BindDouble(index, value);
}

template<> inline int BindImpl(Query & query, size_t index, const std::string & value)
{
	return query.BindString(index, value);
}

template<> inline int BindImpl(Query & query, std::string_view name, const int & value)
{
	return query.BindInt(name, value);
}

template<> inline int BindImpl(Query & query, std::string_view name, const long long int & value)
{
	return query.BindLong(name, value);
}

template<> inline int BindImpl(Query & query, std::string_view name, const double & value)
{
	return query.BindDouble(name, value);
}

template<> inline int BindImpl(Query & query, std::string_view name, const std::string & value)
{
	return query.BindString(name, value);
}

}
