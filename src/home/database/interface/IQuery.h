#pragma once

#include "ICommand.h"

namespace HomeCompa::DB
{

class IQuery;
template <typename T>
T GetImpl(const IQuery& query, size_t index) = delete;

class IQuery : virtual public ICommand
{
public:
	virtual bool Eof()   = 0;
	virtual void Next()  = 0;
	virtual void Reset() = 0;

	virtual size_t      ColumnCount() const            = 0;
	virtual std::string ColumnName(size_t index) const = 0;

	virtual bool          IsNull(size_t index) const       = 0;
	virtual int           GetInt(size_t index) const       = 0;
	virtual long long int GetLong(size_t index) const      = 0;
	virtual double        GetDouble(size_t index) const    = 0;
	virtual std::string   GetString(size_t index) const    = 0;
	virtual const char*   GetRawString(size_t index) const = 0;

	template <typename T>
	T Get(const size_t index) const
	{
		return GetImpl<T>(*this, index);
	}
};

template <>
inline int GetImpl(const IQuery& query, const size_t index)
{
	return query.GetInt(index);
}

template <>
inline long long int GetImpl(const IQuery& query, const size_t index)
{
	return query.GetLong(index);
}

template <>
inline double GetImpl(const IQuery& query, const size_t index)
{
	return query.GetDouble(index);
}

template <>
inline std::string GetImpl(const IQuery& query, const size_t index)
{
	return query.GetString(index);
}

template <>
inline const char* GetImpl(const IQuery& query, const size_t index)
{
	return query.GetRawString(index);
}

} // namespace HomeCompa::DB
