#pragma once

#include <functional>

namespace HomeCompa::Flibrary
{

class ICollectionUpdateChecker // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Callback = std::function<void(bool updateFound, const Collection& updatedCollection)>;

public:
	virtual ~ICollectionUpdateChecker() = default;
	virtual void CheckForUpdate(Callback callback) const = 0;
};

}
