#include <memory>

#include <plog/Log.h>

#include "IExecutor.h"
#include "executor/factory.h"

namespace HomeCompa::Util::ExecutorPrivate::Sync {

namespace {

class Executor
	: virtual public Util::IExecutor
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
		PLOGD << task.name << " started";
		const auto taskResult = task.task();
		PLOGD << task.name << " finished";
		taskResult();
		m_initializer.afterExecute();
	}

private:
	const ExecutorInitializer m_initializer;
};

}

std::unique_ptr<Util::IExecutor> CreateExecutor(ExecutorInitializer initializer)
{
	return std::make_unique<Executor>(std::move(initializer));
}

}
