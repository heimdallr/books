#pragma once

namespace HomeCompa::DB
{

class ITemporaryTable  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~ITemporaryTable() = default;

	virtual const std::string& GetName() const noexcept = 0;
};

}
