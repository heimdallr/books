#pragma once

#include <memory>

#include "UtilLib.h"

namespace HomeCompa::Util {

#define UTIL_EXECUTOR_IMPLS_XMACRO \
		UTIL_EXECUTOR_IMPL(Sync)   \
		UTIL_EXECUTOR_IMPL(Async)

class Executor;
enum class ExecutorImpl
{
#define UTIL_EXECUTOR_IMPL(NAME) NAME,
		UTIL_EXECUTOR_IMPLS_XMACRO
#undef	UTIL_EXECUTOR_IMPL
};

struct ExecutorInitializer
{
	std::function<void()> onCreate { [] {} };
	std::function<void()> beforeExecute { [] {} };
	std::function<void()> afterExecute { [] {} };
	std::function<void()> onDestroy { [] {} };
};

}

namespace HomeCompa::Util::ExecutorFactory {

UTIL_API std::unique_ptr<Executor> Create(ExecutorImpl impl, ExecutorInitializer initializer = {});

}
