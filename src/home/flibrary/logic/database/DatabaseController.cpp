#include "DatabaseController.h"

#include <plog/Log.h>

#include "database/factory/Factory.h"
#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "interface/logic/ICollectionController.h"
#include "interface/logic/ILogicFactory.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

std::unique_ptr<DB::IDatabase> CreateDatabase(const std::string & databaseName)
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
	transaction->Commit();

	return db;
}

}

class DatabaseController::Impl final
	: ICollectionController::IObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	explicit Impl(ILogicFactory & logicFactory, std::shared_ptr<ICollectionController> collectionController)
		: m_logicFactory(logicFactory)
		, m_collectionController(std::move(collectionController))
	{
		m_collectionController->RegisterObserver(this);

		OnActiveCollectionChanged(m_collectionController->GetActiveCollection());
	}

	~Impl() override
	{
		m_collectionController->UnregisterObserver(this);
	}

private: // ICollectionController::IObserver
	void OnActiveCollectionChanged(const Collection & collection) override
	{
		m_logicFactory.SetDatabase(CreateDatabase(collection.database.toStdString()));
	}

private:
	ILogicFactory & m_logicFactory;
	PropagateConstPtr<ICollectionController, std::shared_ptr> m_collectionController;
};

DatabaseController::DatabaseController(ILogicFactory & logicFactory, std::shared_ptr<ICollectionController> collectionController)
	: m_impl(logicFactory, std::move(collectionController))
{
}

DatabaseController::~DatabaseController() = default;
