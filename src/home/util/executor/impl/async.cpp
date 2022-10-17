#include <future>
#include <map>
#include <memory>
#include <mutex>

#include "fnd/NonCopyMovable.h"

#include "executor.h"
#include "FunctorExecutionForwarder.h"

namespace HomeCompa::Util::ExecutorPrivate::Async {

namespace {

class Executor
	: virtual public Util::Executor
{
	NON_COPY_MOVABLE(Executor)

public:
	explicit Executor(std::function<void()> initializer)
		: m_initializer(std::move(initializer))
		, m_thread(&Executor::Work, this)
	{
		m_initializePromise.get_future().get();
		m_startPromise.get_future().get();
	}

	~Executor() override
	{
		m_running = false;
		{
			std::unique_lock lock(m_startMutex);
			m_startCondition.notify_all();
		}

		if (m_thread.joinable())
			m_thread.join();
	}

private: // Util::Executor
	void Execute(Task && task, int priority) override
	{
		{
			std::lock_guard lock(m_tasksGuard);
			m_tasks.insert_or_assign(priority, std::move(task));
		}
		std::unique_lock lock(m_startMutex);
		m_startCondition.notify_one();
	}

private:
	void Work()
	{
		m_initializePromise.set_value();
		const auto initializer = std::move(m_initializer);
		initializer();

		m_startPromise.set_value();
		while (m_running)
		{
			{
				std::unique_lock lock(m_startMutex);
				m_startCondition.wait(lock, [this] ()
				{
					if (!m_running)
						return true;

					std::lock_guard lock(m_tasksGuard);
					return !m_tasks.empty();
				});
			}

			if (!m_running)
				continue;

			auto task = [this] ()
			{
				std::lock_guard lock(m_tasksGuard);
				if (m_tasks.empty())
					return Task([] { return []{}; });

				const auto it = m_tasks.begin();
				auto task = std::move(it->second);
				m_tasks.erase(it);
				return task;
			}();

			auto taskResult = task();
			m_forwarder.Forward(std::move(taskResult));
		}
	}

private:
	std::function<void()> m_initializer;
	std::atomic_bool m_running { true };
	std::mutex m_startMutex;
	std::condition_variable m_startCondition;
	std::promise<void> m_startPromise;
	std::promise<void> m_initializePromise;
	std::thread m_thread;
	std::mutex m_tasksGuard;
	std::map<int, Task> m_tasks;
	FunctorExecutionForwarder m_forwarder;
};

}

std::unique_ptr<Util::Executor> CreateExecutor(std::function<void()> initializer)
{
	return std::make_unique<Executor>(std::move(initializer));
}

}
