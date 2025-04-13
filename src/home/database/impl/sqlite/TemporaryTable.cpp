#include <cassert>
#include <format>
#include <ranges>

#include "fnd/NonCopyMovable.h"

#include "ICommand.h"
#include "IQuery.h"
#include "ITemporaryTable.h"
#include "ITransaction.h"

namespace HomeCompa::DB::Impl::Sqlite
{

namespace
{

int id { 0 };

class TemporaryTable : virtual public ITemporaryTable
{
	NON_COPY_MOVABLE(TemporaryTable)

public:
	TemporaryTable(ITransaction& tr, const std::vector<std::string_view>& fields, const std::vector<std::string_view>& additional)
		: m_tr { tr }
		, m_name { std::format("tab_{}", ++id) }
	{
		assert(!fields.empty());
		std::string statement = "create table ";
		statement.append(m_name).append("(").append(fields.front());

		for (const auto& field : fields | std::views::drop(1))
			statement.append(", ").append(field);
		statement.append(");");

		auto ok = m_tr.CreateCommand(statement)->Execute();
		assert(ok);

		for (const auto add : additional)
		{
			ok = m_tr.CreateCommand(add)->Execute();
			assert(ok);
		}
	}

	~TemporaryTable() override
	{
		m_tr.CreateQuery(std::format("drop table {}", m_name))->Execute();
	}

private: // Transaction
	const std::string& GetName() const noexcept override
	{
		return m_name;
	}

private:
	ITransaction& m_tr;
	const std::string m_name;
};

} // namespace

std::unique_ptr<ITemporaryTable> CreateTemporaryTableImpl(ITransaction& tr, const std::vector<std::string_view>& fields, const std::vector<std::string_view>& additional)
{
	return std::make_unique<TemporaryTable>(tr, fields, additional);
}

} // namespace HomeCompa::DB::Impl::Sqlite
