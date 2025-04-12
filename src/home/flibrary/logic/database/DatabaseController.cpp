#include "DatabaseController.h"

#include "fnd/observable.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/logic/ICollectionProvider.h"

#include "database/factory/Factory.h"
#include "util/FunctorExecutionForwarder.h"

#include "DatabaseScheme.h"
#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

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
		DatabaseScheme::Update(*db, collectionProvider);
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
