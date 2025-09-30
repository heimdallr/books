#pragma once

#include <atomic>
#include <functional>
#include <string>

#include "export/util.h"

namespace HomeCompa::Util
{

class UTIL_EXPORT IExecutor
{
	static std::atomic<size_t> s_id;

public:
	using TaskResult = std::function<void(size_t)>;

	struct Task
	{
		std::string                 name;
		std::function<TaskResult()> task { [] {
			return [](size_t) {
			};
		} };
		size_t id { ++s_id };
	};

public:
	IExecutor()                                              = default;
	IExecutor(const IExecutor&)                              = delete;
	IExecutor(IExecutor&&)                                   = default;
	IExecutor& operator=(const IExecutor&)                   = delete;
	IExecutor& operator=(IExecutor&&)                        = default;
	virtual ~IExecutor()                                     = default;
	virtual size_t operator()(Task&& task, int priority = 0) = 0;

	size_t operator()(Task&& task, const int priority = 0) const
	{
		return (*const_cast<IExecutor*>(this))(std::move(task), priority);
	}

	virtual void Stop() = 0;
};

} // namespace HomeCompa::Util
