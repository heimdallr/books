#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

namespace HomeCompa::Flibrary {

class DatabaseController final
{
	NON_COPY_MOVABLE(DatabaseController)

public:
	explicit DatabaseController(class ILogicFactory & logicFactory, std::shared_ptr<class ICollectionController> collectionController);
	~DatabaseController();

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
