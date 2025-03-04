#include "DatabaseController.h"

#include <QCryptographicHash>

#include <mutex>
#include <ranges>
#include <set>

#include <QFileInfo>

#include "fnd/observable.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "interface/logic/ICollectionProvider.h"

#include "database/factory/Factory.h"
#include "inpx/src/util/constant.h"
#include "inpx/src/util/inpx.h"
#include "inpx/src/util/types.h"
#include "util/FunctorExecutionForwarder.h"

#include "log.h"
#include "zip.h"

using namespace HomeCompa;
using namespace Flibrary;

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
		"CREATE TABLE IF NOT EXISTS Inpx(Folder VARCHAR(200) NOT NULL, File VARCHAR(200) NOT NULL, Hash VARCHAR(50) NOT NULL)",
		"CREATE UNIQUE INDEX IF NOT EXISTS UIX_Inpx_PrimaryKey ON Inpx (Folder COLLATE NOCASE, File COLLATE NOCASE)",
		"CREATE VIRTUAL TABLE IF NOT EXISTS Books_Search USING fts5(Title, content=Books, content_rowid=BookID)",
	};
	// clang-format on

	for (const auto* command : commands)
		transaction.CreateCommand(command)->Execute();
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
	const auto query = transaction.CreateQuery("select count (42) from Books_Search_idx");
	query->Execute();
	if (query->Get<int>(0) == 0)
		transaction.CreateCommand("insert into Books_Search(Books_Search) values('rebuild')")->Execute();
}

void FillInpx(const ICollectionProvider& collectionProvider, DB::ITransaction& transaction)
{
	const auto query = transaction.CreateQuery("select count (42) from Inpx");
	query->Execute();
	if (query->Get<int>(0) == 0)
		Inpx::Parser::FillInpx(collectionProvider.GetActiveCollection().folder.toStdWString(), transaction);
}

std::unique_ptr<DB::IDatabase> CreateDatabaseImpl(const ICollectionProvider& collectionProvider, const bool readOnly)
{
	if (!collectionProvider.ActiveCollectionExists())
		return {};

	const auto databaseName = collectionProvider.GetActiveCollection().database.toStdString();
	if (databaseName.empty())
		return {};

	const auto connectionString = std::string("path=") + databaseName + ";extension=MyHomeLibSQLIteExt" + (readOnly ? ";flag=READONLY" : "");
	auto db = Create(DB::Factory::Impl::Sqlite, connectionString);

	db->CreateQuery("PRAGMA foreign_keys = ON;")->Execute();

	{
		const auto query = db->CreateQuery("select sqlite_version();");
		query->Execute();
		PLOGI << "sqlite version: " << query->Get<std::string>(0);
	}

	if (readOnly)
		return db;

	try
	{
		const auto transaction = db->CreateTransaction();
		DropTriggers(*transaction);
		AddUserTables(*transaction);
		AddUserTableField(*transaction, "Books_User", "UserRate", "INTEGER");
		AddUserTableField(*transaction, "Books_User", "CreatedAt", "DATETIME");
		AddUserTableField(*transaction, "Groups_User", "CreatedAt", "DATETIME");
		AddUserTableField(*transaction, "Groups_List_User", "CreatedAt", "DATETIME");
		AddUserTableField(*transaction, "Searches_User", "CreatedAt", "DATETIME");
		AddUserTableField(*transaction,
		                  "Authors",
		                  "SearchName",
		                  "VARCHAR (128) COLLATE NOCASE",
		                  { "UPDATE Authors SET SearchName = MHL_UPPER(LastName)", "CREATE INDEX IX_Authors_SearchName ON Authors(SearchName COLLATE NOCASE)" });
		AddUserTableField(*transaction,
		                  "Books",
		                  "SearchTitle",
		                  "VARCHAR (150) COLLATE NOCASE",
		                  { "UPDATE Books SET SearchTitle = MHL_UPPER(Title)", "CREATE INDEX IX_Book_SearchTitle ON Books(SearchTitle COLLATE NOCASE)" });
		AddUserTableField(*transaction,
		                  "Keywords",
		                  "SearchTitle",
		                  "VARCHAR (150) COLLATE NOCASE",
		                  { "UPDATE Keywords SET SearchTitle = MHL_UPPER(KeywordTitle)", "CREATE INDEX IX_Keywords_SearchTitle ON Keywords(SearchTitle COLLATE NOCASE)" });
		AddUserTableField(*transaction,
		                  "Series",
		                  "SearchTitle",
		                  "VARCHAR (80) COLLATE NOCASE",
		                  { "UPDATE Series SET SearchTitle = MHL_UPPER(SeriesTitle)", "CREATE INDEX IX_Series_SearchTitle ON Series(SearchTitle COLLATE NOCASE)" });
		if (AddUserTableField(*transaction,
		                      "Books",
		                      "FolderID",
		                      "INTEGER",
		                      { "CREATE INDEX IX_Books_FolderID ON Books(FolderID)",
		                        "CREATE UNIQUE INDEX UIX_Folders_PrimaryKey ON Folders (FolderID)",
		                        "CREATE INDEX IX_Folders_FolderTitle ON Folders(FolderTitle COLLATE NOCASE)" }))
			OnBooksFolderIDAdded(*transaction);

		FixSearches_User(*transaction);
		FillBooksSearch(*transaction);

		FillInpx(collectionProvider, *transaction);

		transaction->Commit();
		return db;
	}
	catch (const std::exception& ex)
	{
		PLOGE << ex.what();
	}
	catch (...)
	{
		PLOGE << "Unknown error";
	}
	return {};
}

} // namespace

class DatabaseController::Impl final
	: ICollectionsObserver
	, public Observable<IObserver>
{
	NON_COPY_MOVABLE(Impl)

public:
	explicit Impl(std::shared_ptr<ICollectionProvider> collectionProvider)
		: m_collectionProvider { std::move(collectionProvider) }
	{
		m_collectionProvider->RegisterObserver(this);

		OnActiveCollectionChanged();
	}

	~Impl() override
	{
		m_collectionProvider->UnregisterObserver(this);
	}

	std::shared_ptr<DB::IDatabase> GetDatabase(const bool create, const bool readOnly) const
	{
		std::lock_guard lock(m_dbGuard);

		if (!create)
			return m_db;

		if (m_db)
			return m_db;

		auto db = CreateDatabaseImpl(*m_collectionProvider, readOnly);
		m_db = std::move(db);

		if (m_db)
		{
			m_forwarder.Forward([&, db = m_db] { const_cast<Impl*>(this)->Perform(&IObserver::AfterDatabaseCreated, std::ref(*db)); });
		}

		return m_db;
	}

private: // ICollectionsObserver
	void OnActiveCollectionChanged() override
	{
		if (m_db)
			Perform(&IObserver::BeforeDatabaseDestroyed, std::ref(*m_db));
		std::lock_guard lock(m_dbGuard);
		m_db.reset();
	}

	void OnNewCollectionCreating(bool) override
	{
	}

private:
	mutable std::mutex m_dbGuard;
	mutable std::shared_ptr<DB::IDatabase> m_db;
	PropagateConstPtr<ICollectionProvider, std::shared_ptr> m_collectionProvider;
	Util::FunctorExecutionForwarder m_forwarder;
};

DatabaseController::DatabaseController(std::shared_ptr<ICollectionProvider> collectionProvider)
	: m_impl(std::move(collectionProvider))
{
	PLOGV << "DatabaseController created";
}

DatabaseController::~DatabaseController()
{
	PLOGV << "DatabaseController destroyed";
}

std::shared_ptr<DB::IDatabase> DatabaseController::GetDatabase(const bool readOnly) const
{
	return m_impl->GetDatabase(true, readOnly);
}

std::shared_ptr<DB::IDatabase> DatabaseController::CheckDatabase() const
{
	return m_impl->GetDatabase(false, true);
}

void DatabaseController::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void DatabaseController::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
