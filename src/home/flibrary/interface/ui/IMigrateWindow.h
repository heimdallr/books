#pragma once

namespace HomeCompa::Flibrary
{

class IMigrateWindow // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IMigrateWindow() = default;

	virtual void Show() = 0;
};

}
