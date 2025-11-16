#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/ICollectionController.h"
#include "interface/logic/ITaskQueue.h"
#include "interface/ui/IUiFactory.h"

#include "util/ISettings.h"

namespace HomeCompa::Flibrary
{

class CollectionController final : public ICollectionController
{
	NON_COPY_MOVABLE(CollectionController)

public:
	CollectionController(std::shared_ptr<ICollectionProvider> collectionProvider, std::shared_ptr<ISettings> settings, std::shared_ptr<IUiFactory> uiFactory, const std::shared_ptr<ITaskQueue>& taskQueue);
	~CollectionController() override;

public: // ICollectionController
	void                             AddCollection(const std::filesystem::path& inpxDir) override;
	void                             RescanCollectionFolder() override;
	void                             RemoveCollection() override;
	[[nodiscard]] bool               IsEmpty() const noexcept override;
	[[nodiscard]] bool               IsCollectionNameExists(const QString& name) const override;
	[[nodiscard]] QString            GetCollectionDatabaseName(const QString& databaseFileName) const override;
	[[nodiscard]] std::set<QString>  GetInpxFiles(const QString& folder) const override;
	[[nodiscard]] bool               IsCollectionFolderHasInpx(const QString& folder) const override;
	[[nodiscard]] Collections&       GetCollections() noexcept override;
	[[nodiscard]] const Collections& GetCollections() const noexcept override;
	[[nodiscard]] Collection&        GetActiveCollection() noexcept override;
	[[nodiscard]] const Collection&  GetActiveCollection() const noexcept override;
	[[nodiscard]] bool               ActiveCollectionExists() const noexcept override;
	[[nodiscard]] QString            GetActiveCollectionId() const noexcept override;
	void                             SetActiveCollection(const QString& id) override;
	void                             OnInpxUpdateChecked(const Collection& updatedCollection) override;
	void                             AllowDestructiveOperation(bool value) override;
	IniMapPair                       GetIniMap(const QString& db, const QString& inpxFolder, bool createFiles) const override;

	void RegisterObserver(ICollectionsObserver* observer) override;
	void UnregisterObserver(ICollectionsObserver* observer) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
