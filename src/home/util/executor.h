#pragma once

#include <functional>

namespace HomeCompa::Util {

class Executor
{
public:
	using TaskResult = std::function<void()>;
	using Task = std::function<TaskResult()>;

public:
	virtual ~Executor() = default;
	virtual void Execute(Task && task) = 0;
};

}
