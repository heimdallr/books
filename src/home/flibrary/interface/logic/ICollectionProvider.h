#pragma once

#include <filesystem>
#include <memory>
#include <set>
#include <vector>

#include <QString>

#include "fnd/observer.h"

#include "inpx/inpx.h"

#include "export/flint.h"

class QTemporaryDir;

namespace HomeCompa::Flibrary
{

struct FLINT_EXPORT Collection
{
	using Ptr = std::unique_ptr<Collection>;

	QString id;
	QString name;
	QString discardedUpdate;
	int     createCollectionMode { 0 };
	bool    destructiveOperationsAllowed { false };

	QString GetDatabase() const;
	QString GetFolder() const;

private:
	QString m_database;
	QString m_folder;
	friend struct CollectionImpl;
};

using Collections = std::vector<Collection::Ptr>;

class ICollectionsObserver : public Observer
{
public:
	virtual void OnActiveCollectionChanged()   = 0;
	virtual void OnNewCollectionCreating(bool) = 0;
};

class ICollectionProvider : public ICollectionsObserver
{
public:
	using IniMapPair = std::pair<std::shared_ptr<QTemporaryDir>, Inpx::Parser::IniMap>;

public:
	[[nodiscard]] virtual bool IsEmpty() const noexcept = 0;

	[[nodiscard]] virtual bool              IsCollectionNameExists(const QString& name) const                = 0;
	[[nodiscard]] virtual QString           GetCollectionDatabaseName(const QString& databaseFileName) const = 0;
	[[nodiscard]] virtual std::set<QString> GetInpxFiles(const QString& archiveFolder) const                 = 0;
	[[nodiscard]] virtual bool              IsCollectionFolderHasInpx(const QString& archiveFolder) const    = 0;

	[[nodiscard]] virtual Collections&       GetCollections() noexcept                                                       = 0;
	[[nodiscard]] virtual const Collections& GetCollections() const noexcept                                                 = 0;
	[[nodiscard]] virtual Collection&        GetActiveCollection() noexcept                                                  = 0;
	[[nodiscard]] virtual const Collection&  GetActiveCollection() const noexcept                                            = 0;
	[[nodiscard]] virtual bool               ActiveCollectionExists() const noexcept                                         = 0;
	[[nodiscard]] virtual QString            GetActiveCollectionId() const noexcept                                          = 0;
	[[nodiscard]] virtual IniMapPair         GetIniMap(const QString& db, const QString& inpxFolder, bool createFiles) const = 0;

	virtual void RegisterObserver(ICollectionsObserver* observer)   = 0;
	virtual void UnregisterObserver(ICollectionsObserver* observer) = 0;

private:
	void OnActiveCollectionChanged() override
	{
	}

	void OnNewCollectionCreating(bool) override
	{
	}
};

} // namespace HomeCompa::Flibrary
