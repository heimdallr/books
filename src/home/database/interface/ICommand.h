#pragma once

#include <string_view>

namespace HomeCompa::DB
{

class ICommand;
template <typename T>
int BindImpl(ICommand& command, size_t index, T value) = delete;
template <typename T>
int BindImpl(ICommand& command, std::string_view name, T value) = delete;
template <typename T>
concept IsString = std::is_same_v<T, const std::string&> || std::is_same_v<T, std::string> || std::is_same_v<T, const char*>;
template <typename T>
concept NotString = !IsString<T>;

class ICommand // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~ICommand() = default;

public:
	virtual bool Execute() = 0;

	virtual int Bind(size_t index)                               = 0;
	virtual int BindInt(size_t index, int value)                 = 0;
	virtual int BindLong(size_t index, long long int value)      = 0;
	virtual int BindDouble(size_t index, double value)           = 0;
	virtual int BindString(size_t index, std::string_view value) = 0;

	template <IsString T>
	int Bind(const size_t index, T value)
	{
		return BindString(index, value);
	}

	template <NotString T>
	int Bind(const size_t index, T value)
	{
		return BindImpl<T>(*this, index, value);
	}

	virtual int Bind(std::string_view name)                               = 0;
	virtual int BindInt(std::string_view name, int value)                 = 0;
	virtual int BindLong(std::string_view name, long long int value)      = 0;
	virtual int BindDouble(std::string_view name, double value)           = 0;
	virtual int BindString(std::string_view name, std::string_view value) = 0;

	template <IsString T>
	int Bind(const std::string_view name, T value)
	{
		return BindString(name, value);
	}

	template <NotString T>
	int Bind(const std::string_view name, T value)
	{
		return BindImpl<T>(*this, name, value);
	}
};

template <>
inline int BindImpl(ICommand& command, const size_t index, const int value)
{
	return command.BindInt(index, value);
}

template <>
inline int BindImpl(ICommand& command, const size_t index, const long long int value)
{
	return command.BindLong(index, value);
}

template <>
inline int BindImpl(ICommand& command, const size_t index, const double value)
{
	return command.BindDouble(index, value);
}

template <>
inline int BindImpl(ICommand& command, const size_t index, const std::string_view value)
{
	return command.BindString(index, value);
}

template <>
inline int BindImpl(ICommand& command, const std::string_view name, const int value)
{
	return command.BindInt(name, value);
}

template <>
inline int BindImpl(ICommand& command, const std::string_view name, const long long int value)
{
	return command.BindLong(name, value);
}

template <>
inline int BindImpl(ICommand& command, const std::string_view name, const double value)
{
	return command.BindDouble(name, value);
}

template <>
inline int BindImpl(ICommand& command, const std::string_view name, const std::string_view value)
{
	return command.BindString(name, value);
}

} // namespace HomeCompa::DB
