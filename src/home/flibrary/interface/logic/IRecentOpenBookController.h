#pragma once

class QMenu;

namespace HomeCompa::Flibrary
{

class IRecentOpenBookController // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IRecentOpenBookController() = default;
	virtual void SetMenu(QMenu* menu)    = 0;
};

}
