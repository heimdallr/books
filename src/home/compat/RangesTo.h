#pragma once

#include <ranges>
#include <utility>

#ifndef __cpp_lib_ranges_zip

#include <range/v3/view/subrange.hpp>
#include <range/v3/view/zip.hpp>

namespace std::ranges
{
namespace flibrary_compat
{
template <viewable_range Range>
auto ToRangeV3(Range&& range)
{
	return ::ranges::make_subrange(begin(range), end(range));
}

struct Zip
{
	template <viewable_range... Ranges>
	auto operator()(Ranges&&... ranges) const
	{
		auto zipped = ::ranges::views::zip(ToRangeV3(std::forward<Ranges>(ranges))...);
		return subrange(zipped.begin(), zipped.end());
	}
};
}

namespace views
{
inline constexpr flibrary_compat::Zip zip {};
}
}

#endif

#ifndef __cpp_lib_ranges_to_container

namespace std::ranges
{
namespace flibrary_compat
{
template <typename Container, input_range Range>
constexpr Container Convert(Range&& range)
{
	if constexpr (common_range<Range>)
	{
		return Container(begin(range), end(range));
	}
	else
	{
		auto common = views::common(std::forward<Range>(range));
		return Container(begin(common), end(common));
	}
}

template <template <typename...> typename Container, input_range Range>
constexpr auto Convert(Range&& range)
{
	if constexpr (common_range<Range>)
	{
		return Container(begin(range), end(range));
	}
	else
	{
		auto common = views::common(std::forward<Range>(range));
		return Container(begin(common), end(common));
	}
}

template <typename Container>
struct ToClosure
{
	template <input_range Range>
	constexpr Container operator()(Range&& range) const
	{
		return Convert<Container>(std::forward<Range>(range));
	}

	template <input_range Range>
	friend constexpr Container operator|(Range&& range, const ToClosure& closure)
	{
		return closure(std::forward<Range>(range));
	}
};

template <template <typename...> typename Container>
struct ToTemplateClosure
{
	template <input_range Range>
	constexpr auto operator()(Range&& range) const
	{
		return Convert<Container>(std::forward<Range>(range));
	}

	template <input_range Range>
	friend constexpr auto operator|(Range&& range, const ToTemplateClosure& closure)
	{
		return closure(std::forward<Range>(range));
	}
};
}

template <typename Container, input_range Range>
constexpr Container to(Range&& range)
{
	return flibrary_compat::Convert<Container>(std::forward<Range>(range));
}

template <template <typename...> typename Container, input_range Range>
constexpr auto to(Range&& range)
{
	return flibrary_compat::Convert<Container>(std::forward<Range>(range));
}

template <typename Container>
constexpr auto to()
{
	return flibrary_compat::ToClosure<Container> {};
}

template <template <typename...> typename Container>
constexpr auto to()
{
	return flibrary_compat::ToTemplateClosure<Container> {};
}
}

#endif
