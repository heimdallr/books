#pragma once

#include <type_traits>

template <typename E>
struct enable_bitmask_operators
{
	static constexpr bool enable = false;
};

template <typename E>
concept BitmaskEnabled = enable_bitmask_operators<E>::enable;

template <BitmaskEnabled E>
constexpr E operator~(E value) noexcept
{
	using underlying = typename std::underlying_type<E>::type;
	return static_cast<E>(~static_cast<underlying>(value));
}

template <BitmaskEnabled E>
constexpr bool operator!(E value) noexcept
{
	using underlying = typename std::underlying_type<E>::type;
	return !static_cast<underlying>(value);
}

template <BitmaskEnabled E>
constexpr E operator|(E lhs, E rhs) noexcept
{
	using underlying = typename std::underlying_type<E>::type;
	return static_cast<E>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

template <BitmaskEnabled E>
constexpr E operator&(E lhs, E rhs) noexcept
{
	using underlying = typename std::underlying_type<E>::type;
	return static_cast<E>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

template <BitmaskEnabled E>
constexpr E operator^(E lhs, E rhs) noexcept
{
	using underlying = typename std::underlying_type<E>::type;
	return static_cast<E>(static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs));
}

template <BitmaskEnabled E>
constexpr E& operator|=(E& lhs, E rhs) noexcept
{
	lhs = lhs | rhs;
	return lhs;
}

template <BitmaskEnabled E>
constexpr E& operator&=(E& lhs, E rhs) noexcept
{
	lhs = lhs & rhs;
	return lhs;
}

template <BitmaskEnabled E>
constexpr E& operator^=(E& lhs, E rhs) noexcept
{
	lhs = lhs ^ rhs;
	return lhs;
}

#define ENABLE_BITMASK_OPERATORS(TYPE)       \
	template <>                              \
	struct enable_bitmask_operators<TYPE>    \
	{                                        \
		static constexpr bool enable = true; \
	}
