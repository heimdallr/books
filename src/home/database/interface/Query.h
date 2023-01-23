#pragma once

#include "Command.h"

namespace HomeCompa::DB {

class Query;
template<typename T> T GetImpl(const Query & query, size_t index) = delete;

class Query
	: virtual public Command
{
public:
	virtual bool Eof() = 0;
	virtual void Next() = 0;

	virtual size_t ColumnCount() const = 0;
	virtual std::string ColumnName(size_t index) const = 0;

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

}
