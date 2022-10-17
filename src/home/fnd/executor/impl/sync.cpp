#include <memory>

#include "executor.h"

namespace HomeCompa::ExecutorPrivate::Sync {

namespace {

class Executor
	: virtual public HomeCompa::Executor
{
	void Execute(Task && task) override
	{
		const auto taskResult = task();
		taskResult();
	}
};

}

std::unique_ptr<HomeCompa::Executor> CreateExecutor()
{
	return std::make_unique<Executor>();
}

}
