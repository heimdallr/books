#pragma once

#include <memory>
#include <vector>

namespace HomeCompa::DB
{

class ICommand;
class IQuery;
class ITemporaryTable;

class ITransaction // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	static constexpr auto DEFAULT_TEMPORARY_TABLE_FIELD = "id integer primary key not null";

public:
	virtual ~ITransaction() = default;

	virtual bool Commit() = 0;
	virtual bool Rollback() = 0;

	virtual [[nodiscard]] std::unique_ptr<ICommand> CreateCommand(std::string_view command) = 0;
	virtual [[nodiscard]] std::unique_ptr<IQuery> CreateQuery(std::string_view command) = 0;
	virtual [[nodiscard]] std::unique_ptr<ITemporaryTable> CreateTemporaryTable(const std::vector<std::string_view>& fields = { DEFAULT_TEMPORARY_TABLE_FIELD },
	                                                                            const std::vector<std::string_view>& additional = {}) = 0;
};

}
