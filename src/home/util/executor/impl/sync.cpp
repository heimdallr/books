#include <memory>

#include "executor.h"

namespace HomeCompa::Util::ExecutorPrivate::Sync {

namespace {

class Executor
	: virtual public Util::Executor
{
public:
	explicit Executor(std::function<void()> initializer)
	{
		const auto f = std::move(initializer);
		f();
	}
private: // Util::Executor
	void operator()(Task && task, int /*priority*/) override
	{
		const auto taskResult = task();
		taskResult();
	}
};

}

std::unique_ptr<Util::Executor> CreateExecutor(std::function<void()> initializer)
{
	return std::make_unique<Executor>(std::move(initializer));
}

}
