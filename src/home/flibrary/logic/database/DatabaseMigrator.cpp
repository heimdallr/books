#include "DatabaseMigrator.h"

#include "fnd/observable.h"

#include "interface/constants/ProductConstant.h"

#include "DatabaseScheme.h"
#include "log.h"

using namespace HomeCompa::Flibrary;

struct DatabaseMigrator::Impl : Observable<IDatabaseMigrator::IObserver>
{
	std::shared_ptr<const IDatabaseProvider>   databaseProvider;
	std::shared_ptr<const ICollectionProvider> collectionProvider;

	Impl(std::shared_ptr<const IDatabaseProvider> databaseProvider, std::shared_ptr<const ICollectionProvider> collectionProvider)
		: databaseProvider { std::move(databaseProvider) }
		, collectionProvider { std::move(collectionProvider) }
	{
	}

	QString GetKey() const
	{
		return QString("Collections/%1/DbSchemeUpdated").arg(collectionProvider->GetActiveCollectionId());
	}
};

DatabaseMigrator::DatabaseMigrator(std::shared_ptr<const IDatabaseProvider> databaseProvider, std::shared_ptr<const ICollectionProvider> collectionProvider)
	: m_impl(std::move(databaseProvider), std::move(collectionProvider))
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

	const auto dbVersion = m_impl->databaseProvider->GetSetting(IDatabaseProvider::Key::DatabaseVersion).toInt();

	return dbVersion == -1                                            ? NeedMigrateResult::Recreate
	     : dbVersion == Constant::FlibraryDatabaseVersionNumber       ? NeedMigrateResult::Actual
	     : dbVersion < Constant::MinimumFlibraryDatabaseVersionNumber ? NeedMigrateResult::Unsupported
	     : dbVersion < Constant::FlibraryDatabaseVersionNumber        ? NeedMigrateResult::NeedMigrate
	                                                                  : (assert(dbVersion > Constant::FlibraryDatabaseVersionNumber && "unexpected result"), NeedMigrateResult::Unexpected);
}

void DatabaseMigrator::Migrate()
{
	m_impl->databaseProvider->ExecuteWritable({ "Database migration", [this](DB::IDatabaseWritable& db) {
												   DatabaseScheme::Update(db, *m_impl->collectionProvider);
												   return [this](size_t) {
													   m_impl->databaseProvider->SetSetting(IDatabaseProvider::Key::DatabaseVersion, Constant::FlibraryDatabaseVersionNumber);
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
