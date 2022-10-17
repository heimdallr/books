#include <memory>

#include "executor.h"

namespace HomeCompa::Util::ExecutorPrivate::Sync {

namespace {

class Executor
	: virtual public Util::Executor
{
	void Execute(Task && task) override
	{
		const auto taskResult = task();
		taskResult();
	}
};

}

std::unique_ptr<Util::Executor> CreateExecutor()
{
	return std::make_unique<Executor>();
}

}
