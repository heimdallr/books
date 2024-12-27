#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "interface/IRequester.h"

namespace HomeCompa::Flibrary {
class ICollectionProvider;
}

namespace HomeCompa::Opds {

class Requester : virtual public IRequester
{
	NON_COPY_MOVABLE(Requester)

public:
	Requester(std::shared_ptr<Flibrary::ICollectionProvider> collectionProvider);
	~Requester() override;

private:
	QByteArray GetRoot() const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
