#pragma once

#include <memory>

#include "fnd/NonCopyMovable.h"

#include "interface/logic/ICollectionProvider.h"

#include "export/logic.h"

namespace HomeCompa
{
class ISettings;
}

namespace HomeCompa::Flibrary
{

struct CollectionImpl;

class LOGIC_EXPORT CollectionProvider final : virtual public ICollectionProvider
{
	NON_COPY_MOVABLE(CollectionProvider)

public:
	explicit CollectionProvider(std::shared_ptr<ISettings> settings);
	~CollectionProvider() override;

private: // ICollectionProvider
	[[nodiscard]] bool IsEmpty() const noexcept override;
	[[nodiscard]] bool IsCollectionNameExists(const QString& name) const override;
	[[nodiscard]] QString GetCollectionDatabaseName(const QString& databaseFileName) const override;
	[[nodiscard]] std::set<QString> GetInpxFiles(const QString& folder) const override;
	[[nodiscard]] bool IsCollectionFolderHasInpx(const QString& folder) const override;
	[[nodiscard]] Collections& GetCollections() noexcept override;
	[[nodiscard]] const Collections& GetCollections() const noexcept override;
	[[nodiscard]] Collection& GetActiveCollection() noexcept override;
	[[nodiscard]] const Collection& GetActiveCollection() const noexcept override;
	[[nodiscard]] bool ActiveCollectionExists() const noexcept override;
	[[nodiscard]] QString GetActiveCollectionId() const noexcept override;

	void RegisterObserver(ICollectionsObserver* observer) override;
	void UnregisterObserver(ICollectionsObserver* observer) override;
	void OnActiveCollectionChanged() override;
	void OnNewCollectionCreating(bool) override;

private:
	class Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
