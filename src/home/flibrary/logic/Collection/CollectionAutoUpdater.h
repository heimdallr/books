#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/ICollectionAutoUpdater.h"
#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/ICollectionUpdateChecker.h"

namespace HomeCompa::Flibrary
{

class CollectionAutoUpdater final : virtual public ICollectionAutoUpdater
{
	NON_COPY_MOVABLE(CollectionAutoUpdater)

public:
	CollectionAutoUpdater(std::shared_ptr<const ICollectionUpdateChecker> collectionUpdateChecker, std::shared_ptr<const ICollectionProvider> collectionProvider);
	~CollectionAutoUpdater() override;

private: // ICollectionAutoUpdater
	void RegisterObserver(IObserver* observer) override;
	void UnregisterObserver(IObserver* observer) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
