#pragma once

#include <atomic>
#include <functional>
#include <string>

#include "UtilLib.h"

namespace HomeCompa::Util {

class UTIL_API IExecutor
{
	static std::atomic<size_t> s_id;

public:
	using TaskResult = std::function<void(size_t)>;
	struct Task
	{
		std::string name;
		std::function<TaskResult()> task { [] { return [](size_t) {}; } };
		size_t id{ ++s_id };
	};

public:
	IExecutor() = default;
	IExecutor(const IExecutor &) = delete;
	IExecutor(IExecutor &&) = default;
	IExecutor & operator=(const IExecutor &) = delete;
	IExecutor & operator=(IExecutor &&) = default;
	virtual ~IExecutor() = default;
	virtual size_t operator()(Task && task, int priority = 0) = 0;
};

}
