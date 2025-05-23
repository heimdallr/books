#pragma once

#include <functional>

namespace HomeCompa::Flibrary
{

class IUpdateChecker // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Callback = std::function<void()>;

public:
	virtual ~IUpdateChecker() = default;
	virtual void CheckForUpdate(bool force, Callback callback) = 0;
};

}
