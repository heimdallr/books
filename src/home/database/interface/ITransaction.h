#pragma once

#include <memory>
#include <string_view>

namespace HomeCompa::DB
{

class ICommand;
class IQuery;

class ITransaction
{
public:
	virtual ~ITransaction() = default;

	virtual bool Commit() = 0;
	virtual bool Rollback() = 0;

	virtual [[nodiscard]] std::unique_ptr<ICommand> CreateCommand(std::string_view command) = 0;
	virtual [[nodiscard]] std::unique_ptr<IQuery> CreateQuery(std::string_view command) = 0;
};

}
