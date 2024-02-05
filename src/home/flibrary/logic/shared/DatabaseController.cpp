#include "DatabaseController.h"

#include <mutex>

#include <plog/Log.h>

#include "fnd/observable.h"
#include "util/FunctorExecutionForwarder.h"

#include "database/factory/Factory.h"
#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "interface/logic/ICollectionController.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

std::unique_ptr<DB::IDatabase> CreateDatabaseImpl(const std::string & databaseName)
{
	if (databaseName.empty())
		return {};

	const std::string connectionString = std::string("path=") + databaseName + ";extension=MyHomeLibSQLIteExt";
	auto db = Create(DB::Factory::Impl::Sqlite, connectionString);

	db->CreateQuery("PRAGMA foreign_keys = ON;")->Execute();

	{
		const auto query = db->CreateQuery("select sqlite_version();");
		query->Execute();
		PLOGI << "sqlite version: " << query->Get<std::string>(0);
	}

	const auto transaction = db->CreateTransaction();
	transaction->CreateCommand("CREATE TABLE IF NOT EXISTS Books_User(BookID INTEGER NOT NULL PRIMARY KEY, IsDeleted INTEGER, FOREIGN KEY(BookID) REFERENCES Books(BookID))")->Execute();
	transaction->CreateCommand("CREATE TABLE IF NOT EXISTS Groups_User(GroupID INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, Title VARCHAR(150) NOT NULL UNIQUE COLLATE MHL_SYSTEM_NOCASE)")->Execute();
	transaction->CreateCommand("CREATE TABLE IF NOT EXISTS Groups_List_User(GroupID INTEGER NOT NULL, BookID INTEGER NOT NULL, PRIMARY KEY(GroupID, BookID), FOREIGN KEY(GroupID) REFERENCES Groups_User(GroupID) ON DELETE CASCADE, FOREIGN KEY(BookID) REFERENCES Books(BookID))")->Execute();
	transaction->CreateCommand("CREATE TABLE IF NOT EXISTS Searches_User(SearchID INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, Title VARCHAR(150) NOT NULL UNIQUE COLLATE MHL_SYSTEM_NOCASE)")->Execute();
	transaction->Commit();

	return db;
}

}

class DatabaseController::Impl final
	: ICollectionController::IObserver
	, public Observable<IObserver>
{
	NON_COPY_MOVABLE(Impl)

public:
	explicit Impl(std::shared_ptr<ICollectionController> collectionController)
		: m_collectionController(std::move(collectionController))
	{
		m_collectionController->RegisterObserver(this);

		OnActiveCollectionChanged();
	}

	~Impl() override
	{
		m_collectionController->UnregisterObserver(this);
	}

	std::shared_ptr<DB::IDatabase> GetDatabase() const
	{
		std::lock_guard lock(m_dbGuard);
		if (m_db)
			return m_db;

		auto db = CreateDatabaseImpl(m_databaseFileName.toStdString());
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

private: // ICollectionController::IObserver
	void OnActiveCollectionChanged() override
	{
		const auto collection = m_collectionController->GetActiveCollection();
		m_databaseFileName = collection ? collection->database : QString{};
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
	PropagateConstPtr<ICollectionController, std::shared_ptr> m_collectionController;
	Util::FunctorExecutionForwarder m_forwarder;
};

DatabaseController::DatabaseController(std::shared_ptr<ICollectionController> collectionController)
	: m_impl(std::move(collectionController))
{
	PLOGD << "DatabaseController created";
}

DatabaseController::~DatabaseController()
{
	PLOGD << "DatabaseController destroyed";
}

std::shared_ptr<DB::IDatabase> DatabaseController::GetDatabase() const
{
	return m_impl->GetDatabase();
}

void DatabaseController::RegisterObserver(IObserver * observer)
{
	m_impl->Register(observer);
}

void DatabaseController::UnregisterObserver(IObserver * observer)
{
	m_impl->Unregister(observer);
}
