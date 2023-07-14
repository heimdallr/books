#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/ICollectionController.h"

#include "logicLib.h"

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Flibrary {

struct Collection;

class LOGIC_API CollectionController final
	: virtual public ICollectionController
{
	NON_COPY_MOVABLE(CollectionController)

public:
	explicit CollectionController(std::shared_ptr<ISettings> settings);
	~CollectionController() override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
