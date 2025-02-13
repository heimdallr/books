#pragma once
#include <memory>

#include "fnd/NonCopyMovable.h"

#include "interface/logic/ICollectionCleaner.h"

namespace HomeCompa::Flibrary
{

class CollectionCleaner : public ICollectionCleaner
{
	NON_COPY_MOVABLE(CollectionCleaner)
public:
	CollectionCleaner(const std::shared_ptr<const class ILogicFactory>& logicFactory,
	                  std::shared_ptr<const class IDatabaseUser> databaseUser,
	                  std::shared_ptr<const class ICollectionProvider> collectionProvider,
	                  std::shared_ptr<class IBooksExtractorProgressController> progressController);
	~CollectionCleaner() override;

private: // ICollectionCleaner
	void Remove(Books books, Callback callback) const override;
	void Analyze(IAnalyzeObserver& observer) const override;
	void AnalyzeCancel() const override;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

}
