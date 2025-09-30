#pragma once

namespace HomeCompa::DB
{

class ITemporaryTable // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~ITemporaryTable() = default;

	virtual const std::string& GetName() const noexcept       = 0;
	virtual const std::string& GetSchemaName() const noexcept = 0;
	virtual const std::string& GetTableName() const noexcept  = 0;
};

}
