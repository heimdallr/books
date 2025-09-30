#pragma once

#pragma warning(push, 0)
#include <Windows.h>
#pragma warning(pop)

template <typename SizeType, typename StringType>
SizeType StrSize(const StringType& str)
{
	return static_cast<SizeType>(std::size(str));
}

template <typename SizeType>
SizeType StrSize(const char* str)
{
	return static_cast<SizeType>(std::strlen(str));
}

template <typename SizeType>
SizeType StrSize(const wchar_t* str)
{
	return static_cast<SizeType>(std::wcslen(str));
}

template <typename StringType>
const char* MultiByteData(const StringType& str)
{
	return str.data();
}

inline const char* MultiByteData(const char* data)
{
	return data;
}

template <typename StringType>
std::wstring ToWide(const StringType& str)
{
	const auto   size = static_cast<std::wstring::size_type>(MultiByteToWideChar(CP_UTF8, 0, MultiByteData(str), StrSize<int>(str), nullptr, 0));
	std::wstring result(size, 0);
	MultiByteToWideChar(CP_UTF8, 0, MultiByteData(str), StrSize<int>(str), result.data(), StrSize<int>(result));

	return result;
}

template <typename StringType>
const wchar_t* WideData(const StringType& str)
{
	return str.data();
}

inline const wchar_t* WideData(const wchar_t* data)
{
	return data;
}

template <typename T>
concept WideStringType = std::is_same_v<T, const std::wstring&> || std::is_same_v<T, std::wstring> || std::is_same_v<T, std::wstring_view> || std::is_same_v<T, const wchar_t*>;

template <WideStringType StringType>
std::string ToMultiByte(const StringType& str)
{
	const auto  size = static_cast<std::wstring::size_type>(WideCharToMultiByte(CP_UTF8, 0, WideData(str), StrSize<int>(str), nullptr, 0, nullptr, nullptr));
	std::string result(size, 0);
	WideCharToMultiByte(CP_UTF8, 0, WideData(str), StrSize<int>(str), result.data(), StrSize<int>(result), nullptr, nullptr);

	return result;
}

template <typename LhsType, typename RhsType>
bool IsStringEqual(LhsType&& lhs, RhsType&& rhs)
{
	return CompareStringW(GetThreadLocale(), NORM_IGNORECASE, WideData(lhs), StrSize<int>(lhs), WideData(rhs), StrSize<int>(rhs)) == CSTR_EQUAL;
}

template <typename LhsType, typename RhsType>
bool IsStringLess(LhsType&& lhs, RhsType&& rhs)
{
	return CompareStringW(GetThreadLocale(), NORM_IGNORECASE, WideData(lhs), StrSize<int>(lhs), WideData(rhs), StrSize<int>(rhs)) - CSTR_EQUAL < 0;
}

template <class T = void>
struct StringLess
{
	typedef T    _FIRST_ARGUMENT_TYPE_NAME;
	typedef T    _SECOND_ARGUMENT_TYPE_NAME;
	typedef bool _RESULT_TYPE_NAME;

	constexpr bool operator()(const T& lhs, const T& rhs) const
	{
		return lhs < rhs;
	}
};

template <>
struct StringLess<void>
{
	template <class LhsType, class RhsType>
	constexpr auto operator()(LhsType&& lhs, RhsType&& rhs) const noexcept(noexcept(static_cast<LhsType&&>(lhs) < static_cast<RhsType&&>(rhs))) // strengthened
		-> decltype(static_cast<LhsType&&>(lhs) < static_cast<RhsType&&>(rhs))
	{
		return IsStringLess(static_cast<LhsType&&>(lhs), static_cast<RhsType&&>(rhs));
	}

	using is_transparent = int;
};

template <typename T>
T To(const std::wstring_view value, T defaultValue = 0)
{
	return value.empty() ? defaultValue : static_cast<T>(_wtoi64(value.data()));
}

template <typename It>
std::wstring_view Next(It& beg, const It end, const wchar_t separator)
{
	beg                          = std::find_if(beg, end, [](const wchar_t c) {
        return !iswspace(c);
    });
	auto                    next = std::find(beg, end, separator);
	const std::wstring_view result(beg, next);
	beg = next != end ? std::next(next) : end;
	return result;
}
