#include "di.h"

#include "logic/di_logic.h"

using namespace HomeCompa::Flibrary::Test;

DI::DI(const std::function<void(Hypodermic::ContainerBuilder& builder, Hypodermic::Container& container)>& init)
{
	Hypodermic::ContainerBuilder builder;
	DiLogic(builder, m_container);
	init(builder, *m_container);
	m_container = builder.build();
}

DI::~DI()
{
	m_container.reset();
}
