#include <cassert>
#include <format>
#include <ranges>

#include "fnd/NonCopyMovable.h"

#include "ICommand.h"
#include "IDatabase.h"
#include "ITemporaryTable.h"
#include "ITransaction.h"

namespace HomeCompa::DB::Impl::Sqlite
{

namespace
{

int id { 0 };

class TemporaryTable final : virtual public ITemporaryTable
{
	NON_COPY_MOVABLE(TemporaryTable)

public:
	TemporaryTable(IDatabase& db, const std::vector<std::string_view>& fields, const std::vector<std::string_view>& additional)
		: m_db { db }
	{
		assert(!fields.empty());
		std::string statement = "create table ";
		statement.append(m_name).append("(").append(fields.front());

		for (const auto& field : fields | std::views::drop(1))
			statement.append(", ").append(field);
		statement.append(");");

		const auto tr = db.CreateTransaction();
		auto       ok = tr->CreateCommand(statement)->Execute();
		assert(ok);

		for (const auto add : additional)
		{
			ok = tr->CreateCommand(add)->Execute();
			assert(ok);
		}
		tr->Commit();
	}

	~TemporaryTable() override
	{
		m_db.CreateTransaction()->CreateCommand(std::format("drop table {}", m_name))->Execute();
	}

private: // Transaction
	const std::string& GetName() const noexcept override
	{
		return m_name;
	}

	const std::string& GetSchemaName() const noexcept override
	{
		return m_schemaName;
	}

	const std::string& GetTableName() const noexcept override
	{
		return m_tableName;
	}

private:
	IDatabase&        m_db;
	const std::string m_schemaName { "tmp" };
	const std::string m_tableName { std::format("tab_{}", ++id) };
	const std::string m_name { std::format("{}.{}", m_schemaName, m_tableName) };
};

} // namespace

std::unique_ptr<ITemporaryTable> CreateTemporaryTableImpl(IDatabase& db, const std::vector<std::string_view>& fields, const std::vector<std::string_view>& additional)
{
	return std::make_unique<TemporaryTable>(db, fields, additional);
}

} // namespace HomeCompa::DB::Impl::Sqlite
