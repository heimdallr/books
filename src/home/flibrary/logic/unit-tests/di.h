#pragma once

#include "Hypodermic/Hypodermic.h"

namespace HomeCompa::Flibrary::Test {

class DI
{
public:
	explicit DI(const std::function<void(Hypodermic::ContainerBuilder& builder, Hypodermic::Container& container)>& init);
	~DI();

public:
	template <typename T>
	std::shared_ptr<T> Resolve() const
	{
		return m_container->resolve<T>();
	}

private:
	std::shared_ptr<Hypodermic::Container> m_container;
};

}
