#pragma once

#include <memory>

namespace Hypodermic
{
class Container;
class ContainerBuilder;
}

namespace HomeCompa::fb2cut
{

void DiInit(Hypodermic::ContainerBuilder& builder, std::shared_ptr<Hypodermic::Container>& container);

}
