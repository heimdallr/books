#include <memory>

#include "executor.h"
#include "executor/factory.h"

namespace HomeCompa::Util::ExecutorPrivate::Sync {

namespace {

class Executor
	: virtual public Util::Executor
{
public:
	explicit Executor(ExecutorInitializer initializer)
		: m_initializer(std::move(initializer))
	{
		m_initializer.onCreate();
	}

	~Executor() override
	{
		m_initializer.onDestroy();
	}

private: // Util::Executor
	void operator()(Task && task, int /*priority*/) override
	{
		m_initializer.beforeExecute();
		const auto taskResult = task();
		taskResult();
		m_initializer.afterExecute();
	}

private:
	const ExecutorInitializer m_initializer;
};

}

std::unique_ptr<Util::Executor> CreateExecutor(ExecutorInitializer initializer)
{
	return std::make_unique<Executor>(std::move(initializer));
}

}
