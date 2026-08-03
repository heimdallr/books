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

constexpr auto CREATE_BOOKS_VIEW = R"(
CREATE VIEW Books_View (
	  BookID,   LibID,   Title,   UpdateDate,   LibRate,   Lang,   Year,   FolderID,                        FileName,   BookSize,   UpdateID,                                        IsDeleted,    UserRate,   SourceLib,   SearchTitle,               BaseFileName,   Ext,                 UserUpdateTime ) AS SELECT 
	b.BookID, b.LibID, b.Title, b.UpdateDate, b.LibRate, b.Lang, b.Year, b.FolderID, b.FileName || b.Ext AS FileName, b.BookSize, b.UpdateID, coalesce(bu.IsDeleted, b.IsDeleted) AS IsDeleted, bu.UserRate, b.SourceLib, b.SearchTitle, b.FileName AS BaseFileName, b.Ext, bu.CreatedAt AS UserUpdateTime
FROM Books b
LEFT JOIN Books_User bu ON bu.BookID = b.BookID
)";

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

/*
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
*/
void AddUserTables(DB::ITransaction& transaction)
{
	PLOGI << "Add tables";
	static constexpr const char* commands[] { "DROP VIEW IF EXISTS Books_View",
		                                      "CREATE INDEX IF NOT EXISTS IX_ExportListUser_ExportType_CreatedAt ON Export_List_User (ExportType, CreatedAt DESC)",
		                                      "CREATE INDEX IF NOT EXISTS IX_Books_User_UserRate ON Books_User (UserRate)",
		                                      CREATE_BOOKS_VIEW,
		                                      "ANALYZE" };

	AddUserTableField(transaction, "Genres", "GenreTitle", "VARCHAR (50)");

	for (const auto* command : commands)
		transaction.CreateCommand(command)->Execute();
}

void AddTableFields(DB::ITransaction& transaction)
{
	PLOGI << "Add columns";
	AddUserTableField(transaction, "Authors", "NickName", "VARCHAR(128)");
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
