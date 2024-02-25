#pragma once

namespace HomeCompa::Flibrary {

class IDatabaseChecker  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IDatabaseChecker() = default;
	virtual bool IsDatabaseValid() const = 0;
};

}
