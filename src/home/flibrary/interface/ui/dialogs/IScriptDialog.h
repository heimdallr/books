#pragma once

namespace HomeCompa::Flibrary {

class IScriptDialog  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IScriptDialog() = default;

public:
	virtual int Exec() = 0;
};

}
