#pragma once

#include <algorithm>
#include <cassert>
#include <functional>
#include <iterator>

//.............................................................................

namespace HomeCompa {

/// Ищем в диапазоне пар по first, возвращаем итератор
template
	< typename InputIterator
	, typename Value = typename std::iterator_traits<InputIterator>::value_type
	, typename KeyEqual = std::equal_to<typename Value::first_type>
	>
InputIterator FindPairIteratorByFirst(InputIterator begin, InputIterator end, const typename Value::first_type & key, KeyEqual comparer = KeyEqual {})
{
	return std::find_if(begin, end, [&] (const auto it)
	{
		return comparer(key, it.first);
	});
}

/// Ищем в диапазоне пар по first, возвращаем итератор
/// Работает с контейнером от begin до end
template
	< typename Container
	, typename Value = typename Container::value_type
	, typename KeyEqual = std::equal_to<typename Value::first_type>
	>
typename Container::const_iterator FindPairIteratorByFirst(const Container & container, const typename Value::first_type & key, KeyEqual comparer = KeyEqual {})
{
	return FindPairIteratorByFirst(container.begin(), container.end(), key, comparer);
}

/// Ищем в диапазоне пар по first, возвращаем итератор
/// Работает со статическим массивом
template
	< typename Value
	, size_t ArraySize
	, typename KeyEqual = std::equal_to<typename Value::first_type>
	>
const Value * FindPairIteratorByFirst(Value(&array)[ArraySize], const typename Value::first_type & key, KeyEqual comparer = KeyEqual {})
{
	return FindPairIteratorByFirst(std::cbegin(array), std::cend(array), key, comparer);
}

//.............................................................................

/// Ищем в диапазоне пар по first, возвращаем second
template
	< typename InputIterator
	, typename Value = typename std::iterator_traits<InputIterator>::value_type
	, typename KeyEqual = std::equal_to<typename Value::first_type>
	>
const typename Value::second_type & FindSecond(InputIterator begin, InputIterator end, const typename Value::first_type & key, KeyEqual comparer = KeyEqual {})
{
	const auto it = FindPairIteratorByFirst(begin, end, key, comparer);
	assert(it != end);
	return it->second;
}

/// Ищем в диапазоне пар по first, возвращаем second
/// Работает с контейнером от begin до end
template
	< typename Container
	, typename Value = typename Container::value_type
	, typename KeyEqual = std::equal_to<typename Value::first_type>
	>
const typename Value::second_type & FindSecond(const Container & container, const typename Value::first_type & key, KeyEqual comparer = KeyEqual {})
{
	return FindSecond(container.begin(), container.end(), key, comparer);
}

/// Ищем в диапазоне пар по first, возвращаем second
/// Работает со статическим массивом
template
	< typename Value
	, size_t ArraySize
	, typename KeyEqual = std::equal_to<typename Value::first_type>
	>
const typename Value::second_type & FindSecond(Value(&array)[ArraySize], const typename Value::first_type & key, KeyEqual comparer = KeyEqual {})
{
	return FindSecond(std::cbegin(array), std::cend(array), key, comparer);
}

//.............................................................................

/// Ищем в диапазоне пар по first, если нашли - возвращаем second, иначе - defaultValue
template
	< typename InputIterator
	, typename Value = typename std::iterator_traits<InputIterator>::value_type
	, typename KeyEqual = std::equal_to<typename Value::first_type>
	>
const typename Value::second_type & FindSecond(InputIterator begin, InputIterator end, const typename Value::first_type & key, const typename Value::second_type & defaultValue, KeyEqual comparer = KeyEqual {})
{
	const auto it = FindPairIteratorByFirst(begin, end, key, comparer);
	return it != end ? it->second : defaultValue;
}

/// Ищем в диапазоне пар по first, если нашли - возвращаем second, иначе - defaultValue
/// Работает с контейнером от begin до end
template
	< typename Container
	, typename Value = typename Container::value_type
	, typename KeyEqual = std::equal_to<typename Value::first_type>
	>
const typename Value::second_type & FindSecond(const Container & container, const typename Value::first_type & key, const typename Value::second_type & defaultValue, KeyEqual comparer = KeyEqual {})
{
	return FindSecond(container.begin(), container.end(), key, defaultValue, comparer);
}

/// Ищем в диапазоне пар по first, если нашли - возвращаем second, иначе - defaultValue
/// Работает со статическим массивом
template
	< typename Value
	, size_t ArraySize
	, typename KeyEqual = std::equal_to<typename Value::first_type>
	>
const typename Value::second_type & FindSecond(Value(&array)[ArraySize], const typename Value::first_type & key, const typename Value::second_type & defaultValue, KeyEqual comparer = KeyEqual {})
{
	return FindSecond(std::cbegin(array), std::cend(array), key, defaultValue, comparer);
}

//.............................................................................

/// Ищем в диапазоне пар по second, возвращаем итератор
template
	< typename InputIterator
	, typename Value = typename std::iterator_traits<InputIterator>::value_type
	, typename KeyEqual = std::equal_to<typename Value::second_type>
	>
InputIterator FindPairIteratorBySecond(InputIterator begin, InputIterator end, const typename Value::second_type & key, KeyEqual comparer = KeyEqual {})
{
	return std::find_if(begin, end, [&] (const auto & it)
	{
		return comparer(key, it.second);
	});
}

/// Ищем в диапазоне пар по second, возвращаем итератор
/// Работает с контейнером от begin до end
template
	< typename Container
	, typename Value = typename Container::value_type
	, typename KeyEqual = std::equal_to<typename Value::second_type>
	>
typename Container::const_iterator FindPairIteratorBySecond(const Container & container, const typename Value::second_type & key, KeyEqual comparer = KeyEqual {})
{
	return FindPairIteratorBySecond(container.begin(), container.end(), key, comparer);
}

/// Ищем в диапазоне пар по second, возвращаем итератор
/// Работает со статическим массивом
template
	< typename Value
	, size_t ArraySize
	, typename KeyEqual = std::equal_to<typename Value::second_type>
	>
const Value * FindPairIteratorBySecond(Value(&array)[ArraySize], const typename Value::second_type & key, KeyEqual comparer = KeyEqual {})
{
	return FindPairIteratorBySecond(std::cbegin(array), std::cend(array), key, comparer);
}

//.............................................................................

/// Ищем в диапазоне пар по second, возвращаем first
template
	< typename InputIterator
	, typename Value = typename std::iterator_traits<InputIterator>::value_type
	, typename KeyEqual = std::equal_to<typename Value::second_type>
	>
const typename Value::first_type & FindFirst(InputIterator begin, InputIterator end, const typename Value::second_type & key, KeyEqual comparer = KeyEqual {})
{
	const auto it = FindPairIteratorBySecond(begin, end, key, comparer);
	assert(it != end);
	return it->first;
}

/// Ищем в диапазоне пар по second, возвращаем first
/// Работает с контейнером от begin до end
template
	< typename Container
	, typename Value = typename Container::value_type
	, typename KeyEqual = std::equal_to<typename Value::second_type>
	>
const typename Value::first_type & FindFirst(const Container & container, const typename Value::second_type & key, KeyEqual comparer = KeyEqual {})
{
	return FindFirst(container.begin(), container.end(), key, comparer);
}

/// Ищем в диапазоне пар по second, возвращаем first
/// Работает со статическим массивом
template
	< typename Value
	, size_t ArraySize
	, typename KeyEqual = std::equal_to<typename Value::second_type>
	>
const typename Value::first_type & FindFirst(Value(&array)[ArraySize], const typename Value::second_type & key, KeyEqual comparer = KeyEqual {})
{
	return FindFirst(std::cbegin(array), std::cend(array), key, comparer);
}

//.............................................................................

/// Ищем в диапазоне пар по second, если нашли - возвращаем first, иначе - defaultValue
template
	< typename InputIterator
	, typename Value = typename std::iterator_traits<InputIterator>::value_type
	, typename KeyEqual = std::equal_to<typename Value::second_type>
	>
const typename Value::first_type & FindFirst(InputIterator begin, InputIterator end, const typename Value::second_type & key, const typename Value::first_type & defaultValue, KeyEqual comparer = KeyEqual {})
{
	const auto it = FindPairIteratorBySecond(begin, end, key, comparer);
	return it != end ? it->first : defaultValue;
}

/// Ищем в диапазоне пар по second, если нашли - возвращаем first, иначе - defaultValue
/// Работает с контейнером от begin до end
template
	< typename Container
	, typename Value = typename Container::value_type
	, typename KeyEqual = std::equal_to<typename Value::second_type>
	>
const typename Value::first_type & FindFirst(const Container & container, const typename Value::second_type & key, const typename Value::first_type & defaultValue, KeyEqual comparer = KeyEqual {})
{
	return FindFirst(container.begin(), container.end(), key, defaultValue, comparer);
}

/// Ищем в диапазоне пар по second, если нашли - возвращаем first, иначе - defaultValue
/// Работает со статическим массивом
template
	< typename Value
	, size_t ArraySize
	, typename KeyEqual = std::equal_to<typename Value::second_type>
	>
const typename Value::first_type & FindFirst(Value(&array)[ArraySize], const typename Value::second_type & key, const typename Value::first_type & defaultValue, KeyEqual comparer = KeyEqual {})
{
	return FindFirst(std::cbegin(array), std::cend(array), key, defaultValue, comparer);
}

//.............................................................................

/// Компаратор для std::reference_wrapper
template <typename T>
struct RefWrapComparer
{
	bool operator()(const T & lh, const std::reference_wrapper<const T> & rh) const { return lh == rh.get(); }
	bool operator()(const std::reference_wrapper<const T> & lh, const T & rh) const { return lh.get() == rh; }
	bool operator()(const std::reference_wrapper<const T> & lh, const std::reference_wrapper<const T> & rh) const { return lh.get() == rh.get(); }
};

using RefWrapComparerString = RefWrapComparer<std::string>;

struct PszComparer
{
	bool operator()(const char * lhs, const char * rhs) const { return strcmp(lhs, rhs) == 0; }
};

struct PszComparerCaseInsensitive
{
	bool operator()(const char * lhs, const char * rhs) const
	{
		while (*lhs && *rhs && std::tolower(*lhs++) == std::tolower(*rhs++))
			;

		return !*lhs && !*rhs;
	}
};

}
