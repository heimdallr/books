#pragma once

#include <functional>
#include <mutex>
#include <queue>
#include <ranges>
#include <thread>

#include "fnd/NonCopyMovable.h"

namespace HomeCompa::Util
{
class ThreadPool
{
	NON_COPY_MOVABLE(ThreadPool)

public:
	explicit ThreadPool(const int numThreads = static_cast<int>(std::thread::hardware_concurrency()))
	{
		m_threads.reserve(static_cast<size_t>(numThreads));
		std::ranges::transform(std::views::iota(0, numThreads), std::back_inserter(m_threads), [this](auto) {
			return std::thread(&ThreadPool::work, this);
		});
	}

	~ThreadPool()
	{
		{
			std::unique_lock lock(m_tasksGuard);
			m_stopped = true;
		}

		m_condition.notify_all();

		for (auto& thread : m_threads)
			thread.join();
	}

	void enqueue(std::function<void()> task)
	{
		{
			std::unique_lock lock(m_tasksGuard);
			m_tasks.emplace(std::move(task));
		}

		m_condition.notify_one();
	}

private:
	std::function<void()> getTask()
	{
		std::unique_lock lock(m_tasksGuard);
		m_condition.wait(lock, [this] {
			return !m_tasks.empty() || m_stopped;
		});

		if (m_stopped && m_tasks.empty())
			return {};

		auto task = std::move(m_tasks.front());
		m_tasks.pop();

		return task;
	}

	void work()
	{
		while (const auto task = getTask())
			task();
	}

private:
	std::vector<std::thread>          m_threads;
	std::queue<std::function<void()>> m_tasks;
	std::mutex                        m_tasksGuard;
	std::condition_variable           m_condition;
	std::atomic_bool                  m_stopped { false };
};

} // namespace HomeCompa::Util
