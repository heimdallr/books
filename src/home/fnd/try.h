#pragma once

#include "log.h"

namespace HomeCompa::Util
{

template <typename R, typename S, typename T>
R Try(const S& name, const T& functor, const std::string_view file, const int line)
{
	try
	{
		PLOGV << name << " started";
		auto result = functor();
		PLOGV << name << " finished";
		return std::forward<R>(result);
	}
	catch (const std::exception& ex)
	{
		PLOGE << std::format("{}, {}: {}", file, line, ex.what());
	}
	catch (...)
	{
		PLOGE << std::format("{}, {}: unknown error", file, line);
	}

	return R {};
}

} // namespace HomeCompa::Util

#define TRY(NAME, FUNCTOR) HomeCompa::Util::Try<std::invoke_result_t<decltype(FUNCTOR)>>(NAME, FUNCTOR, __FILE__, __LINE__)
