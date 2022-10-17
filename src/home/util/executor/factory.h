#pragma once

#include <memory>

#include "UtilLib.h"

namespace HomeCompa::Util {

class Executor;
enum class ExecutorImpl
{
	Sync,
	Async
};

}

namespace HomeCompa::Util::ExecutorFactory {

UTIL_API std::unique_ptr<Executor> Create(ExecutorImpl impl);

}
