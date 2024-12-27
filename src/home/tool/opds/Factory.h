#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "interface/IFactory.h"

namespace Hypodermic {
class Container;
}

namespace HomeCompa::Opds {

class Factory : virtual public IFactory
{
	NON_COPY_MOVABLE(Factory)

public:
	explicit Factory(Hypodermic::Container & container);
	~Factory() override;

private:
	std::shared_ptr<IServer> CreateServer() const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
