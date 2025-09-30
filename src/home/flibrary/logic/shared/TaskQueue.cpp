#include "TaskQueue.h"

#include <queue>

#include "log.h"

using namespace HomeCompa::Flibrary;

struct TaskQueue::Impl
{
	std::queue<Task> tasks;
};

TaskQueue::TaskQueue()  = default;
TaskQueue::~TaskQueue() = default;

void TaskQueue::Enqueue(Task task)
{
	m_impl->tasks.push(std::move(task));
}

void TaskQueue::Execute()
{
	if (m_impl->tasks.empty())
		return;

	PLOGV << "TaskQueue: execute tasks: " << m_impl->tasks.size();

	std::queue<Task> tasks;
	m_impl->tasks.swap(tasks);

	while (!tasks.empty())
	{
		auto task = std::move(tasks.front());
		tasks.pop();
		if (task())
			continue;

		PLOGW << "Task failed, queued up once";
		Enqueue(std::move(task));
	}
}
