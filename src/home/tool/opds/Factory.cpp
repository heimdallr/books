#include "Factory.h"

#include <Hypodermic/Container.h>

#include "interface/IServer.h"

using namespace HomeCompa;
using namespace Opds;

struct Factory::Impl
{
	Hypodermic::Container & container;

	explicit Impl(Hypodermic::Container & container)
		: container { container }
	{
	}
};

Factory::Factory(Hypodermic::Container & container)
	: m_impl(container)
{
}

Factory::~Factory() = default;

std::shared_ptr<IServer> Factory::CreateServer() const
{
	return m_impl->container.resolve<IServer>();
}
