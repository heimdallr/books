#pragma once

#include <functional>
#include <string>

namespace HomeCompa::Util {

class Executor
{
public:
	using TaskResult = std::function<void()>;
	struct Task
	{
		std::string name;
		std::function<TaskResult()> task { [] { return [] {}; } };
	};

public:
	virtual ~Executor() = default;
	virtual void operator()(Task && task, int priority = 0) = 0;
};

}
