#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/ICollectionProvider.h"

#include "export/logic.h"

namespace HomeCompa {
	class ISettings;
}

namespace HomeCompa::Flibrary {

struct CollectionImpl;

class LOGIC_EXPORT CollectionProvider final
	: virtual public ICollectionProvider
{
	NON_COPY_MOVABLE(CollectionProvider)

public:
	explicit CollectionProvider(std::shared_ptr<ISettings> settings);
	~CollectionProvider() override;

private: // ICollectionProvider
	[[nodiscard]] bool IsEmpty() const noexcept override;
	[[nodiscard]] bool IsCollectionNameExists(const QString & name) const override;
	[[nodiscard]] QString GetCollectionDatabaseName(const QString & databaseFileName) const override;
	[[nodiscard]] QString GetInpx(const QString & folder) const override;
	[[nodiscard]] bool IsCollectionFolderHasInpx(const QString & folder) const override;
	[[nodiscard]] Collections & GetCollections() noexcept override;
	[[nodiscard]] const Collections & GetCollections() const noexcept override;
	[[nodiscard]] const Collection& GetActiveCollection() const noexcept override;
	[[nodiscard]] bool ActiveCollectionExists() const noexcept override;
	[[nodiscard]] QString GetActiveCollectionId() const noexcept override;

	void RegisterObserver(ICollectionsObserver * observer) override;
	void UnregisterObserver(ICollectionsObserver * observer) override;
	void OnActiveCollectionChanged() override;
	void OnNewCollectionCreating(bool) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
