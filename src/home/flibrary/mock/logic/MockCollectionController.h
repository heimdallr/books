#pragma once

#include <gmock/gmock.h>

#include "interface/logic/ICollectionController.h"

namespace HomeCompa::Flibrary::Test {

class MockCollectionController : public ICollectionController
{
public: // ICollectionProvider
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

public: // ICollectionController
	MOCK_METHOD(bool, AddCollection, (const std::filesystem::path& inpxDir), (override));
	MOCK_METHOD(void, CreateCollection, (const Collection& collection), (override));
	MOCK_METHOD(void, RescanCollectionFolder, (), (override));
	MOCK_METHOD(void, RemoveCollection, (), (override));
	MOCK_METHOD(void, SetActiveCollection, (const QString& id), (override));
	MOCK_METHOD(bool, OnInpxUpdateChecked, (const Collection& updatedCollection), (override));
	MOCK_METHOD(void, AllowDestructiveOperation, (bool value), (override));
	MOCK_METHOD(Collection::Ptr, CreateCollection, (QString name, QString database, QString folder, QString additionalFolder, QString inpx), (override));
};

static_assert(!std::is_abstract_v<MockCollectionController>, "MockCollectionController is abstract!");

} // namespace HomeCompa::Flibrary::Test
