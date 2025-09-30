#include "DatabaseMigrator.h"

#include "fnd/observable.h"

#include "interface/constants/ProductConstant.h"

#include "DatabaseScheme.h"
#include "log.h"

using namespace HomeCompa::Flibrary;

struct DatabaseMigrator::Impl : Observable<IDatabaseMigrator::IObserver>
{
	std::shared_ptr<const IDatabaseUser>       databaseUser;
	std::shared_ptr<const ICollectionProvider> collectionProvider;

	Impl(std::shared_ptr<const IDatabaseUser> databaseUser, std::shared_ptr<const ICollectionProvider> collectionProvider)
		: databaseUser { std::move(databaseUser) }
		, collectionProvider { std::move(collectionProvider) }
	{
	}

	QString GetKey() const
	{
		return QString("Collections/%1/DbSchemeUpdated").arg(collectionProvider->GetActiveCollectionId());
	}
};

DatabaseMigrator::DatabaseMigrator(std::shared_ptr<const IDatabaseUser> databaseUser, std::shared_ptr<const ICollectionProvider> collectionProvider)
	: m_impl(std::move(databaseUser), std::move(collectionProvider))
{
	PLOGV << "DatabaseMigrator created";
}

DatabaseMigrator::~DatabaseMigrator()
{
	PLOGV << "DatabaseMigrator destroyed";
}

IDatabaseMigrator::NeedMigrateResult DatabaseMigrator::NeedMigrate() const
{
	if (!m_impl->collectionProvider->ActiveCollectionExists())
		return NeedMigrateResult::Actual;

	const auto dbVersion = m_impl->databaseUser->GetSetting(IDatabaseUser::Key::DatabaseVersion).toInt();

	return dbVersion == Constant::FlibraryDatabaseVersionNumber ? NeedMigrateResult::Actual
	     : dbVersion < Constant::FlibraryDatabaseVersionNumber  ? NeedMigrateResult::NeedMigrate
	                                                            : (assert(dbVersion > Constant::FlibraryDatabaseVersionNumber && "unexpected result"), NeedMigrateResult::Unexpected);
}

void DatabaseMigrator::Migrate()
{
	m_impl->databaseUser->Execute({ "Database migration", [this] {
									   const auto db = m_impl->databaseUser->Database();
									   DatabaseScheme::Update(*db, *m_impl->collectionProvider);
									   return [this](size_t) {
										   m_impl->databaseUser->SetSetting(IDatabaseUser::Key::DatabaseVersion, Constant::FlibraryDatabaseVersionNumber);
										   m_impl->Perform(&IObserver::OnMigrationFinished);
									   };
								   } });
}

void DatabaseMigrator::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void DatabaseMigrator::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
