#pragma once

namespace HomeCompa
{

template <typename T>
bool IsOneOf(const T&)
{
	return false;
}

/// @brief Проверить, что x равен хотя бы одному из последующих аргументов.
template <typename T, typename... Args>
bool IsOneOf(const T& x, const Args&... args)
{
	return ((args == x) || ...);
}

}
