#pragma once

#include <algorithm>
#include <cassert>
#include <cctype>
#include <functional>
#include <iterator>
#include <span>
#include <string>

//.............................................................................

template <class Predicate, class Value>
concept BinaryLogicalPredicate = requires(const std::remove_reference_t<Predicate>& predicate, const std::remove_reference_t<Value>& lhs, const std::remove_reference_t<Value>& rhs) {
	{ predicate(lhs, rhs) } -> std::same_as<bool>;
};

/// Ищем в диапазоне пар по first, возвращаем итератор
template <
	typename InputIterator,
	typename Value                                              = typename std::iterator_traits<InputIterator>::value_type,
	BinaryLogicalPredicate<typename Value::first_type> KeyEqual = std::equal_to<typename Value::first_type>>
InputIterator FindPairIteratorByFirst(InputIterator begin, InputIterator end, const typename Value::first_type& key, KeyEqual comparer = KeyEqual {})
{
	return std::find_if(begin, end, [&](const auto it) {
		return comparer(key, it.first);
	});
}

/// Ищем в диапазоне пар по first, возвращаем итератор
/// Работает с контейнером от begin до end
template <typename Container, typename Value = typename Container::value_type, BinaryLogicalPredicate<typename Value::first_type> KeyEqual = std::equal_to<typename Value::first_type>>
typename Container::const_iterator FindPairIteratorByFirst(const Container& container, const typename Value::first_type& key, KeyEqual comparer = KeyEqual {})
{
	return FindPairIteratorByFirst(container.begin(), container.end(), key, comparer);
}

/// Ищем в диапазоне пар по first, возвращаем итератор
/// Работает со статическим массивом
template <typename Value, size_t ArraySize, BinaryLogicalPredicate<typename Value::first_type> KeyEqual = std::equal_to<typename Value::first_type>>
const Value* FindPairIteratorByFirst(Value (&array)[ArraySize], const typename Value::first_type& key, KeyEqual comparer = KeyEqual {})
{
	return FindPairIteratorByFirst(std::cbegin(array), std::cend(array), key, comparer);
}

//.............................................................................

/// Ищем в диапазоне пар по first, возвращаем second
template <
	typename InputIterator,
	typename Value                                              = typename std::iterator_traits<InputIterator>::value_type,
	BinaryLogicalPredicate<typename Value::first_type> KeyEqual = std::equal_to<typename Value::first_type>>
const typename Value::second_type& FindSecond(InputIterator begin, InputIterator end, const typename Value::first_type& key, KeyEqual comparer = KeyEqual {})
{
	const auto it = FindPairIteratorByFirst(begin, end, key, comparer);
	assert(it != end);
	return it->second;
}

/// Ищем в диапазоне пар по first, возвращаем second
/// Работает с контейнером от begin до end
template <typename Container, typename Value = typename Container::value_type, BinaryLogicalPredicate<typename Value::first_type> KeyEqual = std::equal_to<typename Value::first_type>>
const typename Value::second_type& FindSecond(const Container& container, const typename Value::first_type& key, KeyEqual comparer = KeyEqual {})
{
	return FindSecond(container.begin(), container.end(), key, comparer);
}

/// Ищем в диапазоне пар по first, возвращаем second
/// Работает со статическим массивом
template <typename Value, size_t ArraySize, BinaryLogicalPredicate<typename Value::first_type> KeyEqual = std::equal_to<typename Value::first_type>>
const typename Value::second_type& FindSecond(Value (&array)[ArraySize], const typename Value::first_type& key, KeyEqual comparer = KeyEqual {})
{
	return FindSecond(std::cbegin(array), std::cend(array), key, comparer);
}

//.............................................................................

/// Ищем в диапазоне пар по first, если нашли - возвращаем second, иначе - defaultValue
template <
	typename InputIterator,
	typename Value                                              = typename std::iterator_traits<InputIterator>::value_type,
	BinaryLogicalPredicate<typename Value::first_type> KeyEqual = std::equal_to<typename Value::first_type>>
const typename Value::second_type& FindSecond(InputIterator begin, InputIterator end, const typename Value::first_type& key, const typename Value::second_type& defaultValue, KeyEqual comparer = KeyEqual {})
{
	const auto it = FindPairIteratorByFirst(begin, end, key, comparer);
	return it != end ? it->second : defaultValue;
}

/// Ищем в диапазоне пар по first, если нашли - возвращаем second, иначе - defaultValue
/// Работает с контейнером от begin до end
template <typename Container, typename Value = typename Container::value_type, BinaryLogicalPredicate<typename Value::first_type> KeyEqual = std::equal_to<typename Value::first_type>>
const typename Value::second_type& FindSecond(const Container& container, const typename Value::first_type& key, const typename Value::second_type& defaultValue, KeyEqual comparer = KeyEqual {})
{
	return FindSecond(container.begin(), container.end(), key, defaultValue, comparer);
}

/// Ищем в диапазоне пар по first, если нашли - возвращаем second, иначе - defaultValue
/// Работает со статическим массивом
template <typename Value, size_t ArraySize, BinaryLogicalPredicate<typename Value::first_type> KeyEqual = std::equal_to<typename Value::first_type>>
const typename Value::second_type& FindSecond(Value (&array)[ArraySize], const typename Value::first_type& key, const typename Value::second_type& defaultValue, KeyEqual comparer = KeyEqual {})
{
	return FindSecond(std::cbegin(array), std::cend(array), key, defaultValue, comparer);
}

//.............................................................................

/// Ищем в диапазоне пар по second, возвращаем итератор
template <
	typename InputIterator,
	typename Value                                               = typename std::iterator_traits<InputIterator>::value_type,
	BinaryLogicalPredicate<typename Value::second_type> KeyEqual = std::equal_to<typename Value::second_type>>
InputIterator FindPairIteratorBySecond(InputIterator begin, InputIterator end, const typename Value::second_type& key, KeyEqual comparer = KeyEqual {})
{
	return std::find_if(begin, end, [&](const auto& it) {
		return comparer(key, it.second);
	});
}

/// Ищем в диапазоне пар по second, возвращаем итератор
/// Работает с контейнером от begin до end
template <typename Container, typename Value = typename Container::value_type, BinaryLogicalPredicate<typename Value::second_type> KeyEqual = std::equal_to<typename Value::second_type>>
typename Container::const_iterator FindPairIteratorBySecond(const Container& container, const typename Value::second_type& key, KeyEqual comparer = KeyEqual {})
{
	return FindPairIteratorBySecond(container.begin(), container.end(), key, comparer);
}

/// Ищем в диапазоне пар по second, возвращаем итератор
/// Работает со статическим массивом
template <typename Value, size_t ArraySize, BinaryLogicalPredicate<typename Value::second_type> KeyEqual = std::equal_to<typename Value::second_type>>
const Value* FindPairIteratorBySecond(Value (&array)[ArraySize], const typename Value::second_type& key, KeyEqual comparer = KeyEqual {})
{
	return FindPairIteratorBySecond(std::cbegin(array), std::cend(array), key, comparer);
}

//.............................................................................

/// Ищем в диапазоне пар по second, возвращаем first
template <
	typename InputIterator,
	typename Value                                               = typename std::iterator_traits<InputIterator>::value_type,
	BinaryLogicalPredicate<typename Value::second_type> KeyEqual = std::equal_to<typename Value::second_type>>
const typename Value::first_type& FindFirst(InputIterator begin, InputIterator end, const typename Value::second_type& key, KeyEqual comparer = KeyEqual {})
{
	const auto it = FindPairIteratorBySecond(begin, end, key, comparer);
	assert(it != end);
	return it->first;
}

/// Ищем в диапазоне пар по second, возвращаем first
/// Работает с контейнером от begin до end
template <typename Container, typename Value = typename Container::value_type, BinaryLogicalPredicate<typename Value::second_type> KeyEqual = std::equal_to<typename Value::second_type>>
const typename Value::first_type& FindFirst(const Container& container, const typename Value::second_type& key, KeyEqual comparer = KeyEqual {})
{
	return FindFirst(container.begin(), container.end(), key, comparer);
}

/// Ищем в диапазоне пар по second, возвращаем first
/// Работает со статическим массивом
template <typename Value, size_t ArraySize, BinaryLogicalPredicate<typename Value::second_type> KeyEqual = std::equal_to<typename Value::second_type>>
const typename Value::first_type& FindFirst(Value (&array)[ArraySize], const typename Value::second_type& key, KeyEqual comparer = KeyEqual {})
{
	return FindFirst(std::cbegin(array), std::cend(array), key, comparer);
}

//.............................................................................

/// Ищем в диапазоне пар по second, если нашли - возвращаем first, иначе - defaultValue
template <
	typename InputIterator,
	typename Value                                               = typename std::iterator_traits<InputIterator>::value_type,
	BinaryLogicalPredicate<typename Value::second_type> KeyEqual = std::equal_to<typename Value::second_type>>
const typename Value::first_type& FindFirst(InputIterator begin, InputIterator end, const typename Value::second_type& key, const typename Value::first_type& defaultValue, KeyEqual comparer = KeyEqual {})
{
	const auto it = FindPairIteratorBySecond(begin, end, key, comparer);
	return it != end ? it->first : defaultValue;
}

/// Ищем в диапазоне пар по second, если нашли - возвращаем first, иначе - defaultValue
/// Работает с контейнером от begin до end
template <typename Container, typename Value = typename Container::value_type, BinaryLogicalPredicate<typename Value::second_type> KeyEqual = std::equal_to<typename Value::second_type>>
const typename Value::first_type& FindFirst(const Container& container, const typename Value::second_type& key, const typename Value::first_type& defaultValue, KeyEqual comparer = KeyEqual {})
{
	return FindFirst(container.begin(), container.end(), key, defaultValue, comparer);
}

/// Ищем в диапазоне пар по second, если нашли - возвращаем first, иначе - defaultValue
/// Работает со статическим массивом
template <typename Value, size_t ArraySize, BinaryLogicalPredicate<typename Value::second_type> KeyEqual = std::equal_to<typename Value::second_type>>
const typename Value::first_type& FindFirst(Value (&array)[ArraySize], const typename Value::second_type& key, const typename Value::first_type& defaultValue, KeyEqual comparer = KeyEqual {})
{
	return FindFirst(std::cbegin(array), std::cend(array), key, defaultValue, comparer);
}

//.............................................................................

struct PszComparer
{
	bool operator()(std::string_view lhs, std::string_view rhs) const
	{
		return compare(lhs, rhs);
	}

	bool operator()(std::span<const unsigned char> lhs, std::string_view rhs) const
	{
		return compare(lhs, rhs);
	}

	bool operator()(std::string_view lhs, std::span<const unsigned char> rhs) const
	{
		return compare(lhs, rhs);
	}

private:
	template <typename L, typename R>
	bool compare(L lhs, R rhs) const
	{
		const auto [lit, rit] = std::ranges::mismatch(lhs, rhs, [](const unsigned char lhs, const char rhs) {
			return lhs == static_cast<unsigned char>(rhs);
		});

		return lit == lhs.end() && rit == rhs.end() || lit == lhs.end() && *rit == 0 || rit == rhs.end() && *lit == 0;
	}
};

struct PszComparerCaseInsensitive
{
	bool operator()(const char* lhs, const char* rhs) const
	{
		while (*lhs && *rhs && std::tolower(*lhs++) == std::tolower(*rhs++))
			;

		return !*lhs && !*rhs;
	}
};
