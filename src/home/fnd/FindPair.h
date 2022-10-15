#pragma once

#include <algorithm>
#include <cassert>
#include <functional>
#include <iterator>

#include <type_traits>

//.............................................................................

namespace HomeCompa {

/// SFINAE для компаратора: должен быть доступен bool operator()(const T&, const &T);
template <typename FuncType, typename ArgType>
struct isBoolExecutableWithTwoSameTypeParams
{
private:
	static void detect(...);
	template <typename U, typename V>
	static decltype(std::declval<U>()(std::declval<V>(), std::declval<V>())) detect(const U &, const V &);

public:
	static constexpr bool value = std::is_same<bool, decltype(detect(std::declval<FuncType>(), std::declval<ArgType>()))>::value;
};

//.............................................................................

/// Ищем в диапазоне пар по first, возвращаем итератор
template
	< typename InputIterator
	, typename Value = typename std::iterator_traits<InputIterator>::value_type
	, typename KeyEqual = std::equal_to<typename Value::first_type>
	>
const typename std::enable_if_t<isBoolExecutableWithTwoSameTypeParams<KeyEqual, typename Value::first_type>::value, InputIterator>
FindPairIteratorByFirst(InputIterator begin, InputIterator end, const typename Value::first_type & key, KeyEqual comparer = KeyEqual {})
{
	return std::find_if(begin, end, std::bind(comparer, key, std::bind(&Value::first, std::placeholders::_1)));
}

/// Ищем в диапазоне пар по first, возвращаем итератор
/// Работает с контейнером от begin до end
template
	< typename Container
	, typename Value = typename Container::value_type
	, typename KeyEqual = std::equal_to<typename Value::first_type>
	>
const typename std::enable_if_t<isBoolExecutableWithTwoSameTypeParams<KeyEqual, typename Value::first_type>::value,
typename Container::const_iterator> FindPairIteratorByFirst(const Container & container, const typename Value::first_type & key, KeyEqual comparer = KeyEqual {})
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
const typename std::enable_if_t<isBoolExecutableWithTwoSameTypeParams<KeyEqual, typename Value::first_type>::value, Value *>
FindPairIteratorByFirst(Value(&array)[ArraySize], const typename Value::first_type & key, KeyEqual comparer = KeyEqual {})
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
const typename std::enable_if_t<isBoolExecutableWithTwoSameTypeParams<KeyEqual, typename Value::first_type>::value, const typename Value::second_type &>
FindSecond(InputIterator begin, InputIterator end, const typename Value::first_type & key, KeyEqual comparer = KeyEqual {})
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
const typename std::enable_if_t<isBoolExecutableWithTwoSameTypeParams<KeyEqual, typename Value::first_type>::value, const typename Value::second_type &>
FindSecond(const Container & container, const typename Value::first_type & key, KeyEqual comparer = KeyEqual {})
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
const typename std::enable_if_t<isBoolExecutableWithTwoSameTypeParams<KeyEqual, typename Value::first_type>::value, const typename Value::second_type &>
FindSecond(Value(&array)[ArraySize], const typename Value::first_type & key, KeyEqual comparer = KeyEqual {})
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
const typename std::enable_if_t<isBoolExecutableWithTwoSameTypeParams<KeyEqual, typename Value::first_type>::value, const typename Value::second_type &>
FindSecond(InputIterator begin, InputIterator end, const typename Value::first_type & key, const typename Value::second_type & defaultValue, KeyEqual comparer = KeyEqual {})
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
const typename std::enable_if_t<isBoolExecutableWithTwoSameTypeParams<KeyEqual, typename Value::first_type>::value, const typename Value::second_type &>
FindSecond(const Container & container, const typename Value::first_type & key, const typename Value::second_type & defaultValue, KeyEqual comparer = KeyEqual {})
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
const typename std::enable_if_t<isBoolExecutableWithTwoSameTypeParams<KeyEqual, typename Value::first_type>::value, const typename Value::second_type &>
FindSecond(Value(&array)[ArraySize], const typename Value::first_type & key, const typename Value::second_type & defaultValue, KeyEqual comparer = KeyEqual {})
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
const typename std::enable_if_t<isBoolExecutableWithTwoSameTypeParams<KeyEqual, typename Value::second_type>::value, InputIterator>
FindPairIteratorBySecond(InputIterator begin, InputIterator end, const typename Value::second_type & key, KeyEqual comparer = KeyEqual {})
{
	return std::find_if(begin, end, std::bind(comparer, key, std::bind(&Value::second, std::placeholders::_1)));
}

/// Ищем в диапазоне пар по second, возвращаем итератор
/// Работает с контейнером от begin до end
template
	< typename Container
	, typename Value = typename Container::value_type
	, typename KeyEqual = std::equal_to<typename Value::second_type>
	>
const typename std::enable_if_t<isBoolExecutableWithTwoSameTypeParams<KeyEqual, typename Value::second_type>::value, typename Container::const_iterator>
FindPairIteratorBySecond(const Container & container, const typename Value::second_type & key, KeyEqual comparer = KeyEqual {})
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
const typename std::enable_if_t<isBoolExecutableWithTwoSameTypeParams<KeyEqual, typename Value::second_type>::value, Value *>
FindPairIteratorBySecond(Value(&array)[ArraySize], const typename Value::second_type & key, KeyEqual comparer = KeyEqual {})
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
const typename std::enable_if_t<isBoolExecutableWithTwoSameTypeParams<KeyEqual, typename Value::second_type>::value, const typename Value::first_type &>
FindFirst(InputIterator begin, InputIterator end, const typename Value::second_type & key, KeyEqual comparer = KeyEqual {})
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
const typename std::enable_if_t<isBoolExecutableWithTwoSameTypeParams<KeyEqual, typename Value::second_type>::value, const typename Value::first_type &>
FindFirst(const Container & container, const typename Value::second_type & key, KeyEqual comparer = KeyEqual {})
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
const typename std::enable_if_t<isBoolExecutableWithTwoSameTypeParams<KeyEqual, typename Value::second_type>::value, const typename Value::first_type &>
FindFirst(Value(&array)[ArraySize], const typename Value::second_type & key, KeyEqual comparer = KeyEqual {})
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
const typename std::enable_if_t<isBoolExecutableWithTwoSameTypeParams<KeyEqual, typename Value::second_type>::value, const typename Value::first_type &>
FindFirst(InputIterator begin, InputIterator end, const typename Value::second_type & key, const typename Value::first_type & defaultValue, KeyEqual comparer = KeyEqual {})
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
const typename std::enable_if_t<isBoolExecutableWithTwoSameTypeParams<KeyEqual, typename Value::second_type>::value, const typename Value::first_type &>
FindFirst(const Container & container, const typename Value::second_type & key, const typename Value::first_type & defaultValue, KeyEqual comparer = KeyEqual {})
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
const typename std::enable_if_t<isBoolExecutableWithTwoSameTypeParams<KeyEqual, typename Value::second_type>::value, const typename Value::first_type &>
FindFirst(Value(&array)[ArraySize], const typename Value::second_type & key, const typename Value::first_type & defaultValue, KeyEqual comparer = KeyEqual {})
{
	return FindFirst(std::cbegin(array), std::cend(array), key, defaultValue, comparer);
}

//.............................................................................

/// Компаратор для std::reference_wrapper
template <typename T>
struct RefWrapComparer
{
	inline bool operator()(const T & lh, const std::reference_wrapper<const T> & rh) const { return lh == rh.get(); }
	inline bool operator()(const std::reference_wrapper<const T> & lh, const T & rh) const { return lh.get() == rh; }
	inline bool operator()(const std::reference_wrapper<const T> & lh, const std::reference_wrapper<const T> & rh) const { return lh.get() == rh.get(); }
};

using RefWrapComparerString = RefWrapComparer<std::string>;

struct PszComparer
{
	bool operator()(const char * lhs, const char * rhs) const { return strcmp(lhs, rhs) == 0; }
};

}
