#pragma once

#include <vector>

namespace HomeCompa::Util {

template<typename T>
static bool Set(T & dst, T value)
{
	if (dst == value)
		return false;

	dst = std::move(value);
	return true;
}

template
	< typename T
	, typename Obj
	, typename Signal = void(Obj::*)()
	>
static bool Set(T & dst, T value, Obj & obj, const Signal signal = nullptr)
{
	if (!Set<T>(dst, std::move(value)))
		return false;

	if (signal)
		(obj.*signal)();

	return true;
}

template
	< typename T
	, typename Obj
	, typename Signal = void(Obj::*)() const
	>
static bool Set(T & dst, T value, const Obj & obj, const Signal signal = nullptr)
{
	if (!Set<T>(dst, std::move(value)))
		return false;

	if (signal)
		(obj.*signal)();

	return true;
}

// из отсортированного диапазона делает вектор диапазонов из идущих подряд элементов
template <typename InputIterator, typename Value = typename std::iterator_traits<InputIterator>::value_type>
std::vector<std::pair<Value, Value>> CreateRanges(InputIterator begin, InputIterator end)
{
	assert(std::is_sorted(begin, end));
	if (begin == end)
		return {};

	std::vector<std::pair<Value, Value>> result{{*begin, *begin + 1}};
	for (++begin; begin != end; ++begin)
		if (*begin == result.back().second)
			++result.back().second;
		else
			result.emplace_back(*begin, *begin + 1);

	return result;
}

template<typename Container, typename ValueType = typename Container::value_type>
std::vector<std::pair<ValueType, ValueType>> CreateRanges(const Container & container)
{
	return Util::CreateRanges(std::cbegin(container), std::cend(container));
}

}
