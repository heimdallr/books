#pragma once

#include <filesystem>
#include <memory>
#include <vector>

#include <QString>
#include <set>

#include "fnd/observer.h"

namespace HomeCompa::Flibrary {

struct Collection
{
	QString id;
	QString name;
	QString database;
	QString folder;
	QString discardedUpdate;
	int createCollectionMode;

	using Ptr = std::unique_ptr<Collection>;
};
using Collections = std::vector<Collection::Ptr>;

class ICollectionsObserver : public Observer
{
public:
	virtual void OnActiveCollectionChanged() = 0;
	virtual void OnNewCollectionCreating(bool) = 0;
};

class ICollectionProvider : public ICollectionsObserver
{
public:
	[[nodiscard]] virtual bool IsEmpty() const noexcept = 0;

	[[nodiscard]] virtual bool IsCollectionNameExists(const QString& name) const = 0;
	[[nodiscard]] virtual QString GetCollectionDatabaseName(const QString & databaseFileName) const = 0;
	[[nodiscard]] virtual std::set<QString> GetInpxFiles(const QString & archiveFolder) const = 0;
	[[nodiscard]] virtual bool IsCollectionFolderHasInpx(const QString & archiveFolder) const = 0;

	[[nodiscard]] virtual Collections & GetCollections() noexcept = 0;
	[[nodiscard]] virtual const Collections & GetCollections() const noexcept = 0;
	[[nodiscard]] virtual const Collection& GetActiveCollection() const noexcept = 0;
	[[nodiscard]] virtual bool ActiveCollectionExists() const noexcept = 0;
	[[nodiscard]] virtual QString GetActiveCollectionId() const noexcept = 0;

	virtual void RegisterObserver(ICollectionsObserver * observer) = 0;
	virtual void UnregisterObserver(ICollectionsObserver * observer) = 0;

private:
	void OnActiveCollectionChanged() override {}
	void OnNewCollectionCreating(bool) override {}
};

}
