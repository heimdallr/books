#include "DatabaseScheme.h"

#include <format>
#include <ranges>
#include <set>

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "interface/logic/ICollectionProvider.h"

#include "inpx/src/util/constant.h"
#include "inpx/src/util/inpx.h"

namespace HomeCompa::Flibrary::DatabaseScheme
{

namespace
{

void DropTriggers(DB::ITransaction& transaction)
{
	std::vector<std::string> triggerNames;
	{
		const auto triggerNamesQuery = transaction.CreateQuery("select name from sqlite_master where type = 'trigger'");
		for (triggerNamesQuery->Execute(); !triggerNamesQuery->Eof(); triggerNamesQuery->Next())
			triggerNames.emplace_back(triggerNamesQuery->Get<const char*>(0));
	}

	for (const auto& triggerName : triggerNames)
		transaction.CreateCommand(std::format("drop trigger if exists {}", triggerName))->Execute();
}

bool FieldExists(DB::ITransaction& transaction, const QString& table, const QString& column)
{
	std::set<std::string> booksUserFields;
	const auto booksUserFieldsQuery = transaction.CreateQuery(QString("PRAGMA table_info(%1)").arg(table).toStdString());
	auto range = std::views::iota(std::size_t { 0 }, booksUserFieldsQuery->ColumnCount());
	const auto it = std::ranges::find(range, "name", [&](const size_t n) { return booksUserFieldsQuery->ColumnName(n); });
	assert(it != std::end(range));
	for (booksUserFieldsQuery->Execute(); !booksUserFieldsQuery->Eof(); booksUserFieldsQuery->Next())
		booksUserFields.emplace(booksUserFieldsQuery->GetString(*it));
	return booksUserFields.contains(column.toStdString());
}

bool AddUserTableField(DB::ITransaction& transaction, const QString& table, const QString& column, const QString& definition, const std::vector<std::string_view>& commands = {})
{
	if (FieldExists(transaction, table, column))
		return false;

	transaction.CreateCommand(QString("ALTER TABLE %1 ADD COLUMN %2 %3").arg(table).arg(column).arg(definition).toStdString())->Execute();
	for (const auto& command : commands)
		transaction.CreateCommand(command)->Execute();

	return true;
}

void OnBooksFolderIDAdded(DB::ITransaction& transaction)
{
	auto maxId = [&]
	{
		const auto query = transaction.CreateQuery(GET_MAX_ID_QUERY);
		query->Execute();
		assert(!query->Eof());
		return query->Get<long long>(0);
	}();

	std::unordered_map<std::string, long long> folders;
	std::vector<std::pair<long long, std::string>> books;

	{
		const auto query = transaction.CreateQuery("select BookID, Folder from Books");
		for (query->Execute(); !query->Eof(); query->Next())
		{
			const auto& [id, folder] = books.emplace_back(query->Get<long long>(0), query->Get<const char*>(1));
			if (auto [it, ok] = folders.try_emplace(folder, 0); ok)
				it->second = ++maxId;
		}
	}
	{
		const auto command = transaction.CreateCommand("insert into Folders(FolderID, FolderTitle) values(?, ?)");
		for (const auto& [folder, id] : folders)
		{
			command->Bind(0, id);
			command->Bind(1, folder);
			command->Execute();
		}
	}
	{
		const auto command = transaction.CreateCommand("update Books set FolderID = ? where BookID = ?");
		for (const auto& [id, folder] : books)
		{
			const auto it = folders.find(folder);
			assert(it != folders.end());
			command->Bind(0, it->second);
			command->Bind(1, id);
			command->Execute();
		}
	}
}

void FixSearches_User(DB::ITransaction& transaction)
{
	if (!FieldExists(transaction, "Searches_User", "Mode"))
		return;

	static constexpr const char* commands[] {
		"DROP TABLE Searches_User",
		"CREATE TABLE Searches_User(SearchID INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, Title VARCHAR (150) NOT NULL UNIQUE COLLATE MHL_SYSTEM_NOCASE, CreatedAt DATETIME)",
	};

	for (const auto* command : commands)
		transaction.CreateCommand(command)->Execute();
}

void FillBooksSearch(DB::ITransaction& transaction)
{
	const auto query = transaction.CreateQuery("SELECT exists(SELECT 1 FROM Books_Search_idx)");
	query->Execute();
	if (query->Get<int>(0) == 0)
		transaction.CreateCommand("insert into Books_Search(Books_Search) values('rebuild')")->Execute();
}

void FillInpx(const ICollectionProvider& collectionProvider, DB::ITransaction& transaction)
{
	const auto query = transaction.CreateQuery("SELECT exists(SELECT 1 FROM Inpx)");
	query->Execute();
	if (query->Get<int>(0) == 0)
		Inpx::Parser::FillInpx(collectionProvider.GetActiveCollection().folder.toStdWString(), transaction);
}

void FillSeriesList(DB::ITransaction& transaction)
{
	const auto query = transaction.CreateQuery("SELECT exists(SELECT 1 FROM Series_List)");
	query->Execute();
	if (query->Get<int>(0) == 0)
		transaction.CreateCommand("insert into Series_List(SeriesID, BookID, SeqNumber) select b.SeriesID, b.BookID, b.SeqNumber from Books b where b.SeriesID is not null")->Execute();
}

void AddUserTables(DB::ITransaction& transaction)
{
	// clang-format off
	static constexpr const char* commands[] {
		"CREATE TABLE IF NOT EXISTS Books_User(BookID INTEGER NOT NULL PRIMARY KEY, IsDeleted INTEGER, UserRate INTEGER, FOREIGN KEY(BookID) REFERENCES Books(BookID))",
		"CREATE TABLE IF NOT EXISTS Groups_User(GroupID INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, Title VARCHAR(150) NOT NULL UNIQUE COLLATE MHL_SYSTEM_NOCASE)",
		"CREATE TABLE IF NOT EXISTS Groups_List_User(GroupID INTEGER NOT NULL, BookID INTEGER NOT NULL, PRIMARY KEY(GroupID, BookID), FOREIGN KEY(GroupID) REFERENCES Groups_User(GroupID) ON DELETE CASCADE, FOREIGN KEY(BookID) REFERENCES Books(BookID))",
		"CREATE TABLE IF NOT EXISTS Searches_User(SearchID INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, Title VARCHAR(150) NOT NULL UNIQUE COLLATE MHL_SYSTEM_NOCASE)",
		"CREATE TABLE IF NOT EXISTS Keywords(KeywordID INTEGER NOT NULL, KeywordTitle VARCHAR(150) NOT NULL COLLATE MHL_SYSTEM_NOCASE)",
		"CREATE TABLE IF NOT EXISTS Keyword_List(KeywordID INTEGER NOT NULL, BookID INTEGER NOT NULL)",
		"CREATE TABLE IF NOT EXISTS Export_List_User(BookID INTEGER NOT NULL, ExportType INTEGER NOT NULL, CreatedAt DATETIME NOT NULL)",
		"CREATE TABLE IF NOT EXISTS Folders(FolderID INTEGER NOT NULL, FolderTitle VARCHAR(200) NOT NULL COLLATE MHL_SYSTEM_NOCASE)",
		"CREATE TABLE IF NOT EXISTS Inpx(Folder VARCHAR(200) NOT NULL, File VARCHAR(200) NOT NULL, Hash VARCHAR(50) NOT NULL)", "CREATE UNIQUE INDEX IF NOT EXISTS UIX_Inpx_PrimaryKey ON Inpx (Folder COLLATE NOCASE, File COLLATE NOCASE)",
		"CREATE TABLE IF NOT EXISTS Series_List (SeriesID  INTEGER NOT NULL, BookID    INTEGER NOT NULL, SeqNumber INTEGER)", "CREATE UNIQUE INDEX IF NOT EXISTS UIX_Series_List_PrimaryKey ON Series_List (SeriesID, BookID)",
		"CREATE TABLE IF NOT EXISTS Updates (UpdateID INTEGER NOT NULL, UpdateTitle INTEGER NOT NULL, ParentID INTEGER NOT NULL)", "CREATE UNIQUE INDEX IF NOT EXISTS UIX_Update_PrimaryKey ON Updates (UpdateID)", "CREATE INDEX IF NOT EXISTS IX_Update_ParentID ON Updates (ParentID)",
		"CREATE VIRTUAL TABLE IF NOT EXISTS Books_Search USING fts5(Title, content=Books, content_rowid=BookID)",
	};
	// clang-format on

	for (const auto* command : commands)
		transaction.CreateCommand(command)->Execute();
}

void AddTableFields(DB::ITransaction& transaction)
{
	AddUserTableField(transaction, "Books_User", "UserRate", "INTEGER");
	AddUserTableField(transaction, "Books_User", "CreatedAt", "DATETIME");
	AddUserTableField(transaction, "Groups_User", "CreatedAt", "DATETIME");
	AddUserTableField(transaction, "Groups_List_User", "CreatedAt", "DATETIME");
	AddUserTableField(transaction, "Searches_User", "CreatedAt", "DATETIME");
	AddUserTableField(transaction,
	                  "Authors",
	                  "SearchName",
	                  "VARCHAR (128) COLLATE NOCASE",
	                  { "UPDATE Authors SET SearchName = MHL_UPPER(LastName)", "CREATE INDEX IX_Authors_SearchName ON Authors(SearchName COLLATE NOCASE)" });
	AddUserTableField(transaction,
	                  "Books",
	                  "SearchTitle",
	                  "VARCHAR (150) COLLATE NOCASE",
	                  { "UPDATE Books SET SearchTitle = MHL_UPPER(Title)", "CREATE INDEX IX_Book_SearchTitle ON Books(SearchTitle COLLATE NOCASE)" });
	AddUserTableField(transaction,
	                  "Keywords",
	                  "SearchTitle",
	                  "VARCHAR (150) COLLATE NOCASE",
	                  { "UPDATE Keywords SET SearchTitle = MHL_UPPER(KeywordTitle)", "CREATE INDEX IX_Keywords_SearchTitle ON Keywords(SearchTitle COLLATE NOCASE)" });
	AddUserTableField(transaction,
	                  "Series",
	                  "SearchTitle",
	                  "VARCHAR (80) COLLATE NOCASE",
	                  { "UPDATE Series SET SearchTitle = MHL_UPPER(SeriesTitle)", "CREATE INDEX IX_Series_SearchTitle ON Series(SearchTitle COLLATE NOCASE)" });
	if (AddUserTableField(transaction,
	                      "Books",
	                      "FolderID",
	                      "INTEGER",
	                      { "CREATE INDEX IX_Books_FolderID ON Books(FolderID)",
	                        "CREATE UNIQUE INDEX UIX_Folders_PrimaryKey ON Folders (FolderID)",
	                        "CREATE INDEX IX_Folders_FolderTitle ON Folders(FolderTitle COLLATE NOCASE)" }))
		OnBooksFolderIDAdded(transaction);
	AddUserTableField(transaction, "Books", "UpdateID", "INTEGER NOT NULL DEFAULT(0)", { "CREATE INDEX IX_Books_UpdateID ON Books(UpdateID)" });
	AddUserTableField(transaction, "Authors", "IsDeleted", "INTEGER NOT NULL DEFAULT(0)");
	AddUserTableField(transaction, "Folders", "IsDeleted", "INTEGER NOT NULL DEFAULT(0)");
	AddUserTableField(transaction, "Genres", "IsDeleted", "INTEGER NOT NULL DEFAULT(0)");
	AddUserTableField(transaction, "Groups_User", "IsDeleted", "INTEGER NOT NULL DEFAULT(0)");
	AddUserTableField(transaction, "Keywords", "IsDeleted", "INTEGER NOT NULL DEFAULT(0)");
	AddUserTableField(transaction, "Series", "IsDeleted", "INTEGER NOT NULL DEFAULT(0)");
	AddUserTableField(transaction, "Updates", "IsDeleted", "INTEGER NOT NULL DEFAULT(0)");
}

} // namespace

void Update(DB::IDatabase& db, const ICollectionProvider& collectionProvider)
{
	const auto transaction = db.CreateTransaction();
	DropTriggers(*transaction);
	AddUserTables(*transaction);
	AddTableFields(*transaction);
	FixSearches_User(*transaction);
	FillBooksSearch(*transaction);
	FillSeriesList(*transaction);
	FillInpx(collectionProvider, *transaction);

	transaction->Commit();
}

} // namespace HomeCompa::Flibrary::DatabaseScheme
