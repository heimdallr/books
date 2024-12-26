#include "DatabaseController.h"

#include <mutex>
#include <ranges>
#include <set>

#include <plog/Log.h>

#include "fnd/observable.h"
#include "util/FunctorExecutionForwarder.h"

#include "database/factory/Factory.h"
#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "interface/logic/ICollectionProvider.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

void AddUserTables(DB::ITransaction & transaction)
{
	transaction.CreateCommand("CREATE TABLE IF NOT EXISTS Books_User(BookID INTEGER NOT NULL PRIMARY KEY, IsDeleted INTEGER, UserRate INTEGER, FOREIGN KEY(BookID) REFERENCES Books(BookID))")->Execute();
	transaction.CreateCommand("CREATE TABLE IF NOT EXISTS Groups_User(GroupID INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, Title VARCHAR(150) NOT NULL UNIQUE COLLATE MHL_SYSTEM_NOCASE)")->Execute();
	transaction.CreateCommand("CREATE TABLE IF NOT EXISTS Groups_List_User(GroupID INTEGER NOT NULL, BookID INTEGER NOT NULL, PRIMARY KEY(GroupID, BookID), FOREIGN KEY(GroupID) REFERENCES Groups_User(GroupID) ON DELETE CASCADE, FOREIGN KEY(BookID) REFERENCES Books(BookID))")->Execute();
	transaction.CreateCommand("CREATE TABLE IF NOT EXISTS Searches_User(SearchID INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, Title VARCHAR(150) NOT NULL UNIQUE COLLATE MHL_SYSTEM_NOCASE)")->Execute();
	transaction.CreateCommand("CREATE TABLE IF NOT EXISTS Keywords(KeywordID INTEGER NOT NULL, KeywordTitle VARCHAR(150) NOT NULL COLLATE MHL_SYSTEM_NOCASE)")->Execute();
	transaction.CreateCommand("CREATE TABLE IF NOT EXISTS Keyword_List(KeywordID INTEGER NOT NULL, BookID INTEGER NOT NULL)")->Execute();
	transaction.CreateCommand("CREATE TABLE IF NOT EXISTS Export_List_User(BookID INTEGER NOT NULL, ExportType INTEGER NOT NULL, CreatedAt DATETIME NOT NULL)")->Execute();
}

void AddUserTableField(DB::ITransaction & transaction, const QString & table, const QString & column, const QString & definition)
{
	std::set<std::string> booksUserFields;
	const auto booksUserFieldsQuery = transaction.CreateQuery(QString("PRAGMA table_info(%1)").arg(table).toStdString());
	auto range = std::views::iota(std::size_t { 0 }, booksUserFieldsQuery->ColumnCount());
	const auto it = std::ranges::find(range, "name", [&] (const size_t n) { return booksUserFieldsQuery->ColumnName(n); });
	assert(it != std::end(range));
	for (booksUserFieldsQuery->Execute(); !booksUserFieldsQuery->Eof(); booksUserFieldsQuery->Next())
		booksUserFields.emplace(booksUserFieldsQuery->GetString(*it));
	if (!booksUserFields.contains(column.toStdString()))
		transaction.CreateCommand(QString("ALTER TABLE %1 ADD COLUMN %2 %3").arg(table).arg(column).arg(definition).toStdString())->Execute();
}

std::unique_ptr<DB::IDatabase> CreateDatabaseImpl(const std::string & databaseName, const bool readOnly)
{
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
		AddUserTables(*transaction);
		AddUserTableField(*transaction, "Books_User", "UserRate", "INTEGER");
		AddUserTableField(*transaction, "Books_User", "CreatedAt", "DATETIME");
		AddUserTableField(*transaction, "Groups_User", "CreatedAt", "DATETIME");
		AddUserTableField(*transaction, "Groups_List_User", "CreatedAt", "DATETIME");
		AddUserTableField(*transaction, "Searches_User", "CreatedAt", "DATETIME");

		transaction->Commit();
		return db;
	}
	catch(const std::exception & ex)
	{
		PLOGE << ex.what();
	}
	catch(...)
	{
		PLOGE << "Unknown error";
	}
	return {};
}

}

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

		auto db = CreateDatabaseImpl(m_databaseFileName.toStdString(), readOnly);
		m_db = std::move(db);

		if (m_db)
		{
			m_forwarder.Forward([&, db = m_db]
			{
				const_cast<Impl *>(this)->Perform(&DatabaseController::IObserver::AfterDatabaseCreated, std::ref(*db));
			});
		}

		return m_db;
	}

private: // ICollectionsObserver
	void OnActiveCollectionChanged() override
	{
		m_databaseFileName = m_collectionProvider->ActiveCollectionExists() ? m_collectionProvider->GetActiveCollection().database : QString {};
		if (m_db)
			Perform(&DatabaseController::IObserver::BeforeDatabaseDestroyed, std::ref(*m_db));
		std::lock_guard lock(m_dbGuard);
		m_db.reset();
	}

	void OnNewCollectionCreating(bool) override
	{
	}

private:
	mutable std::mutex m_dbGuard;
	mutable std::shared_ptr<DB::IDatabase> m_db;
	QString m_databaseFileName;
	PropagateConstPtr<ICollectionProvider, std::shared_ptr> m_collectionProvider;
	Util::FunctorExecutionForwarder m_forwarder;
};

DatabaseController::DatabaseController(std::shared_ptr<ICollectionProvider> collectionProvider)
	: m_impl(std::move(collectionProvider))
{
	PLOGD << "DatabaseController created";
}

DatabaseController::~DatabaseController()
{
	PLOGD << "DatabaseController destroyed";
}

std::shared_ptr<DB::IDatabase> DatabaseController::GetDatabase(const bool readOnly) const
{
	return m_impl->GetDatabase(true, readOnly);
}

std::shared_ptr<DB::IDatabase> DatabaseController::CheckDatabase() const
{
	return m_impl->GetDatabase(false, true);
}

void DatabaseController::RegisterObserver(IObserver * observer)
{
	m_impl->Register(observer);
}

void DatabaseController::UnregisterObserver(IObserver * observer)
{
	m_impl->Unregister(observer);
}
