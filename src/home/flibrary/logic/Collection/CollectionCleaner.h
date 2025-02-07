#pragma once
#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "interface/logic/ICollectionCleaner.h"

namespace HomeCompa::Flibrary {

class CollectionCleaner : public ICollectionCleaner
{
public:
	CollectionCleaner(const std::shared_ptr<const class ILogicFactory>& logicFactory
		, std::shared_ptr<const class IDatabaseUser> databaseUser
		, std::shared_ptr<const class ICollectionProvider> collectionProvider
		, std::shared_ptr<class IBooksExtractorProgressController> progressController
	);
	~CollectionCleaner() override;

private: // ICollectionCleaner
	void Remove(Books books, Callback callback) override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
