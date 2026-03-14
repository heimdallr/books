#include "DatabaseScheme.h"

#include <format>
#include <ranges>
#include <set>

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "interface/logic/ICollectionProvider.h"

#include "inpx/InpxConstant.h"
#include "inpx/inpx.h"

#include "log.h"

namespace HomeCompa::Flibrary::DatabaseScheme
{

namespace
{

bool FieldExists(DB::ITransaction& transaction, const QString& table, const QString& column)
{
	std::set<std::string> booksUserFields;
	const auto            booksUserFieldsQuery = transaction.CreateQuery(QString("PRAGMA table_info(%1)").arg(table).toStdString());
	auto                  range                = std::views::iota(std::size_t { 0 }, booksUserFieldsQuery->ColumnCount());
	const auto            it                   = std::ranges::find(range, "name", [&](const size_t n) {
        return booksUserFieldsQuery->ColumnName(n);
    });
	assert(it != std::end(range));
	for (booksUserFieldsQuery->Execute(); !booksUserFieldsQuery->Eof(); booksUserFieldsQuery->Next())
		booksUserFields.emplace(booksUserFieldsQuery->GetString(*it));
	return booksUserFields.contains(column.toStdString());
}

bool AddUserTableField(DB::ITransaction& transaction, const QString& table, const QString& column, const QString& definition, const std::vector<std::string_view>& commands = {})
{
	if (FieldExists(transaction, table, column))
		return false;

	PLOGI << "Add " << column << " to " << table;

	transaction.CreateCommand(QString("ALTER TABLE %1 ADD COLUMN %2 %3").arg(table).arg(column).arg(definition).toStdString())->Execute();
	for (const auto& command : commands)
		transaction.CreateCommand(command)->Execute();

	return true;
}

long long GetNextID(DB::ITransaction& transaction)
{
	const auto query = transaction.CreateQuery(GET_MAX_ID_QUERY);
	query->Execute();
	assert(!query->Eof());
	return query->Get<long long>(0);
}

bool RecordsExists(DB::ITransaction& transaction, const std::string_view tableName, const std::string_view where = {})
{
	const auto query = transaction.CreateQuery(std::format("SELECT exists(SELECT 42 FROM {} {})", tableName, where));
	query->Execute();
	return query->Get<int>(0) != 0;
}

void AddUserTables(DB::ITransaction& /*transaction*/)
{
	PLOGI << "Add tables";
	//static constexpr const char* commands[] {
	//};

	//for (const auto* command : commands)
	//	transaction.CreateCommand(command)->Execute();
}

void AddTableFields(DB::ITransaction& /*transaction*/)
{
	PLOGI << "Add columns";
}

} // namespace

void Update(DB::IDatabase& db, const ICollectionProvider& /*collectionProvider*/)
{
	const auto transaction = db.CreateTransaction();
	AddUserTables(*transaction);
	AddTableFields(*transaction);

	transaction->Commit();
}

} // namespace HomeCompa::Flibrary::DatabaseScheme
