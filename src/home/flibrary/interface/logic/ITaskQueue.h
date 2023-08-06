#pragma once

#include <functional>

namespace HomeCompa::Flibrary {

class ITaskQueue  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Task = std::function<bool()>;

public:
	virtual ~ITaskQueue() = default;
	virtual void Enqueue(Task task) = 0;
	virtual void Execute() = 0;
};

}
