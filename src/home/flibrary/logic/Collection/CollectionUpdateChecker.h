#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "interface/logic/ICollectionUpdateChecker.h"

namespace HomeCompa::Flibrary {

class CollectionUpdateChecker final
	: virtual public ICollectionUpdateChecker
{
	NON_COPY_MOVABLE(CollectionUpdateChecker)
public:
	CollectionUpdateChecker(std::shared_ptr<class ICollectionController> collectionController
		, std::shared_ptr<class IDatabaseUser> databaseUser
	);
	~CollectionUpdateChecker() override;

private: // ICollectionUpdateChecker
	void CheckForUpdate(Callback callback) const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
