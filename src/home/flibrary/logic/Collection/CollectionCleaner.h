#pragma once
#include <memory>

#include "fnd/NonCopyMovable.h"

#include "interface/logic/ICollectionCleaner.h"
#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IProgressController.h"

namespace HomeCompa::Flibrary
{

class CollectionCleaner : public ICollectionCleaner
{
	NON_COPY_MOVABLE(CollectionCleaner)
public:
	CollectionCleaner(const std::shared_ptr<const ILogicFactory>& logicFactory,
	                  std::shared_ptr<const IDatabaseUser> databaseUser,
	                  std::shared_ptr<const ICollectionProvider> collectionProvider,
	                  std::shared_ptr<IBooksExtractorProgressController> progressController);
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
