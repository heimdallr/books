#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/ITaskQueue.h"

namespace HomeCompa::Flibrary {

class TaskQueue : virtual public ITaskQueue
{
	NON_COPY_MOVABLE(TaskQueue)

public:
	TaskQueue();
	~TaskQueue() override;

private:
	void Enqueue(Task task) override;
	void Execute() override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
