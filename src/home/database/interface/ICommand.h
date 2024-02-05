#pragma once

#include <string_view>

namespace HomeCompa::DB {

class ICommand;
template<typename T> int BindImpl(ICommand & command, size_t index, const T & value) = delete;
template<typename T> int BindImpl(ICommand & command, std::string_view name, const T & value) = delete;

class ICommand  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~ICommand() = default;

public:
	virtual void Execute() = 0;

	virtual int BindInt(size_t index, int value) = 0;
	virtual int BindLong(size_t index, long long int value) = 0;
	virtual int BindDouble(size_t index, double value) = 0;
	virtual int BindString(size_t index, const std::string & value) = 0;

	template<typename T>
	int Bind(const size_t index, const T & value)
	{
		return BindImpl<T>(*this, index, value);
	}

	virtual int BindInt(std::string_view name, int value) = 0;
	virtual int BindLong(std::string_view name, long long int value) = 0;
	virtual int BindDouble(std::string_view name, double value) = 0;
	virtual int BindString(std::string_view name, const std::string & value) = 0;

	template<typename T>
	int Bind(const std::string_view name, const T & value)
	{
		return BindImpl<T>(*this, name, value);
	}
};

template<> inline int BindImpl(ICommand & command, const size_t index, const int & value)
{
	return command.BindInt(index, value);
}

template<> inline int BindImpl(ICommand & command, const size_t index, const long long int & value)
{
	return command.BindLong(index, value);
}

template<> inline int BindImpl(ICommand & command, const size_t index, const double & value)
{
	return command.BindDouble(index, value);
}

template<> inline int BindImpl(ICommand & command, const size_t index, const std::string & value)
{
	return command.BindString(index, value);
}

template<> inline int BindImpl(ICommand & command, const std::string_view name, const int & value)
{
	return command.BindInt(name, value);
}

template<> inline int BindImpl(ICommand & command, const std::string_view name, const long long int & value)
{
	return command.BindLong(name, value);
}

template<> inline int BindImpl(ICommand & command, const std::string_view name, const double & value)
{
	return command.BindDouble(name, value);
}

template<> inline int BindImpl(ICommand & command, const std::string_view name, const std::string & value)
{
	return command.BindString(name, value);
}

}
