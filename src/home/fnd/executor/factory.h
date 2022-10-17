#pragma once

#include <memory>

#include "FndLib.h"

namespace HomeCompa {
class Executor;
enum class ExecutorImpl
{
	Sync,
	Async
};

}

namespace HomeCompa::ExecutorFactory {

FND_API std::unique_ptr<Executor> Create(ExecutorImpl impl);

}
