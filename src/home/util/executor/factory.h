#pragma once

#include <functional>
#include <memory>

#include "export/util.h"

namespace HomeCompa::Util
{

#define UTIL_EXECUTOR_IMPLS_XMACRO \
	UTIL_EXECUTOR_IMPL(Sync)       \
	UTIL_EXECUTOR_IMPL(Async)

class IExecutor;
enum class ExecutorImpl
{
#define UTIL_EXECUTOR_IMPL(NAME) NAME,
	UTIL_EXECUTOR_IMPLS_XMACRO
#undef UTIL_EXECUTOR_IMPL
};

struct ExecutorInitializer
{
	int                   maxThreadCount = 1;
	std::function<void()> onCreate { [] {
	} };
	std::function<void()> beforeExecute { [] {
	} };
	std::function<void()> afterExecute { [] {
	} };
	std::function<void()> onDestroy { [] {
	} };
};

}

namespace HomeCompa::Util::ExecutorFactory
{

UTIL_EXPORT std::unique_ptr<IExecutor> Create(ExecutorImpl impl, ExecutorInitializer initializer = {});

}
