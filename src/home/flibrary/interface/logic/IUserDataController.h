#pragma once

#include <functional>

namespace HomeCompa::Flibrary {

class IUserDataController  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Callback = std::function<void()>;

public:
	virtual ~IUserDataController() = default;
	virtual void Backup(Callback callback) const = 0;
	virtual void Restore(Callback callback) const = 0;
};

}
