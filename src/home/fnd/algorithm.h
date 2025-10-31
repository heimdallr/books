#pragma once

#include <algorithm>
#include <vector>

namespace HomeCompa::Util
{

template <typename T>
static bool Set(T& dst, T value)
{
	if (dst == value)
		return false;

	dst = std::move(value);
	return true;
}

template <typename T, typename Obj, typename Signal = void (Obj::*)()>
static bool Set(T& dst, T value, Obj& obj, const Signal signal)
{
	if (!Set<T>(dst, std::move(value)))
		return false;

	if (signal)
		(obj.*signal)();

	return true;
}

template <typename T, typename Obj, typename Signal = void (Obj::*)() const>
static bool Set(T& dst, T value, const Obj& obj, const Signal signal)
{
	if (!Set<T>(dst, std::move(value)))
		return false;

	if (signal)
		(obj.*signal)();

	return true;
}

template <typename T, std::invocable F>
bool Set(T& oldValue, T&& newValue, const F& f)
{
	if (oldValue == newValue)
		return false;

	oldValue = std::forward<T>(newValue);
	f();
	return true;
}

template <typename T, std::invocable BeforeF, std::invocable AfterF>
bool Set(T& oldValue, T&& newValue, const BeforeF& before, const AfterF& after)
{
	if (oldValue == newValue)
		return false;

	before();
	oldValue = std::forward<T>(newValue);
	after();
	return true;
}

// из отсортированного диапазона делает вектор диапазонов из идущих подряд элементов
template <typename InputIterator, typename Value = typename std::iterator_traits<InputIterator>::value_type>
std::vector<std::pair<Value, Value>> CreateRanges(InputIterator begin, InputIterator end)
{
	assert(std::is_sorted(begin, end));
	if (begin == end)
		return {};

	std::vector<std::pair<Value, Value>> result {
		{ *begin, *begin + 1 }
	};
	for (++begin; begin != end; ++begin)
		if (*begin == result.back().second)
			++result.back().second;
		else
			result.emplace_back(*begin, *begin + 1);

	return result;
}

template <typename Container, typename ValueType = typename Container::value_type>
std::vector<std::pair<ValueType, ValueType>> CreateRanges(const Container& container)
{
	return Util::CreateRanges(std::cbegin(container), std::cend(container));
}

template <class InputIt1, class InputIt2>
bool Intersect(InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2)
{
	assert(std::is_sorted(first1, last1) && std::is_sorted(first2, last2));
	while (first1 != last1 && first2 != last2)
	{
		if (*first1 < *first2)
		{
			++first1;
			continue;
		}
		if (*first2 < *first1)
		{
			++first2;
			continue;
		}
		return true;
	}
	return false;
}

template <typename Container1, typename Container2>
bool Intersect(const Container1& container1, const Container2& container2)
{
	return Intersect(std::cbegin(container1), std::cend(container1), std::cbegin(container2), std::cend(container2));
}

} // namespace HomeCompa::Util
