#pragma once

#include <filesystem>
#include <memory>
#include <vector>

#include <QString>

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

class ICollectionController  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IObserver : public Observer
	{
	public:
		virtual void OnActiveCollectionChanged() = 0;
		virtual void OnNewCollectionCreating(bool) = 0;
	};
public:
	virtual ~ICollectionController() = default;

public:
	virtual void AddCollection(const std::filesystem::path & inpx) = 0;
	virtual void RemoveCollection() = 0;

	[[nodiscard]] virtual bool IsEmpty() const noexcept = 0;

	[[nodiscard]] virtual bool IsCollectionNameExists(const QString& name) const = 0;
	[[nodiscard]] virtual QString GetCollectionDatabaseName(const QString & databaseFileName) const = 0;
	[[nodiscard]] virtual QString GetInpx(const QString & archiveFolder) const = 0;
	[[nodiscard]] virtual bool IsCollectionFolderHasInpx(const QString & archiveFolder) const = 0;

	[[nodiscard]] virtual const Collections & GetCollections() const noexcept = 0;
	[[nodiscard]] virtual const Collection& GetActiveCollection() const noexcept = 0;
	[[nodiscard]] virtual bool ActiveCollectionExists() const noexcept = 0;
	[[nodiscard]] virtual QString GetActiveCollectionId() const noexcept = 0;
	virtual void SetActiveCollection(const QString & id) = 0;
	virtual void OnInpxUpdateFound(const Collection & updatedCollection) = 0;

	virtual void RegisterObserver(IObserver * observer) = 0;
	virtual void UnregisterObserver(IObserver * observer) = 0;
};

}
