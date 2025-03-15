#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/ICollectionController.h"
#include "interface/logic/ICollectionUpdateChecker.h"
#include "interface/logic/IDatabaseUser.h"

namespace HomeCompa::Flibrary
{

class CollectionUpdateChecker final : virtual public ICollectionUpdateChecker
{
	NON_COPY_MOVABLE(CollectionUpdateChecker)
public:
	CollectionUpdateChecker(std::shared_ptr<ICollectionController> collectionController, std::shared_ptr<IDatabaseUser> databaseUser);
	~CollectionUpdateChecker() override;

private: // ICollectionUpdateChecker
	void CheckForUpdate(Callback callback) const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
