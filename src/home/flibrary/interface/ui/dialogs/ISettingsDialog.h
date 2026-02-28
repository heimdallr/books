#pragma once

namespace HomeCompa::Flibrary
{

class ISettingsDialog // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~ISettingsDialog() = default;

public:
	virtual int Exec() = 0;
};

}
