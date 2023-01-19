#pragma once

#include <memory>
#include <string_view>

namespace HomeCompa::DB {

class Command;
class Query;

class Transaction
{
public:
	virtual ~Transaction() = default;

	virtual void Commit() = 0;
	virtual void Rollback() = 0;

	virtual [[nodiscard]] std::unique_ptr<Command> CreateCommand(const std::string_view & command) = 0;
	virtual [[nodiscard]] std::unique_ptr<Query> CreateQuery(const std::string_view & command) = 0;
};

}
