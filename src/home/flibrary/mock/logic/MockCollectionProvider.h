#include <gmock/gmock.h>

#include "interface/logic/ICollectionProvider.h"

namespace HomeCompa::Flibrary::Test {

class MockCollectionProvider : public ICollectionProvider
{
public:
	MOCK_METHOD(bool, IsEmpty, (), (const, noexcept, override));
	MOCK_METHOD(bool, IsCollectionNameExists, (const QString& name), (const, override));
	MOCK_METHOD(QString, GetCollectionDatabaseName, (const QString& databaseFileName), (const, override));
	MOCK_METHOD(std::set<QString>, GetInpxFiles, (const QString& archiveFolder), (const, override));
	MOCK_METHOD(bool, IsCollectionFolderHasInpx, (const QString& archiveFolder), (const, override));
	MOCK_METHOD(Collections&, GetCollections, (), (noexcept, override));
	MOCK_METHOD(const Collections&, GetCollections, (), (const, noexcept, override));
	MOCK_METHOD(Collection&, GetActiveCollection, (), (noexcept, override));
	MOCK_METHOD(const Collection&, GetActiveCollection, (), (const, noexcept, override));
	MOCK_METHOD(bool, ActiveCollectionExists, (), (const, noexcept, override));
	MOCK_METHOD(QString, GetActiveCollectionId, (), (const, noexcept, override));
	MOCK_METHOD(QStringList, GetCollectionStatistics, (DB::IDatabase & db, bool withAdditionalInfo), (const, override));
	MOCK_METHOD(void, RegisterObserver, (ICollectionsObserver * observer), (override));
	MOCK_METHOD(void, UnregisterObserver, (ICollectionsObserver * observer), (override));
};

static_assert(!std::is_abstract_v<MockCollectionProvider>, "MockCollectionProvider is abstract!");

} // namespace HomeCompa::Flibrary::Test
