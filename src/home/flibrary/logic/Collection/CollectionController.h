#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/ICollectionController.h"

#include "export/logic.h"

namespace HomeCompa {
	class ISettings;
}

namespace HomeCompa::Flibrary {

struct CollectionImpl;

class LOGIC_EXPORT CollectionController final
	: virtual public ICollectionController
{
	NON_COPY_MOVABLE(CollectionController)

public:
	CollectionController(std::shared_ptr<ISettings> settings
		, std::shared_ptr<class IUiFactory> uiFactory
		, const std::shared_ptr<class ITaskQueue>& taskQueue
	);
	~CollectionController() override;

public: // ICollectionController
	void AddCollection(const std::filesystem::path & inpx) override;
	void RemoveCollection() override;
	[[nodiscard]] bool IsEmpty() const noexcept override;
	[[nodiscard]] bool IsCollectionNameExists(const QString & name) const override;
	[[nodiscard]] QString GetCollectionDatabaseName(const QString & databaseFileName) const override;
	[[nodiscard]] QString GetInpx(const QString & folder) const override;
	[[nodiscard]] bool IsCollectionFolderHasInpx(const QString & folder) const override;
	[[nodiscard]] const Collections & GetCollections() const noexcept override;
	[[nodiscard]] const Collection& GetActiveCollection() const noexcept override;
	[[nodiscard]] bool ActiveCollectionExists() const noexcept override;
	[[nodiscard]] QString GetActiveCollectionId() const noexcept override;
	void SetActiveCollection(const QString & id) override;
	void OnInpxUpdateFound(const Collection & updatedCollection) override;

	void RegisterObserver(IObserver * observer) override;
	void UnregisterObserver(IObserver * observer) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
