#pragma once

class QMainWindow;

namespace HomeCompa::Flibrary
{

class IMainWindow // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IMainWindow() = default;

	virtual void Show() = 0;
};

}
