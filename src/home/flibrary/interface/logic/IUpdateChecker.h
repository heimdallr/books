#pragma once

#include <functional>

namespace HomeCompa::RestAPI::Github {
struct Release;
}

namespace HomeCompa::Flibrary {

class IUpdateChecker  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Callback = std::function<void(RestAPI::Github::Release latestRelease)>;

public:
	virtual ~IUpdateChecker() = default;
	virtual void CheckForUpdate(Callback callback) = 0;
};

}
