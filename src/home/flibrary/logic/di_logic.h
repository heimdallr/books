#pragma once

#include <memory>

#include "export/logic.h"

namespace Hypodermic
{
class Container;
class ContainerBuilder;
}

namespace HomeCompa::Flibrary
{

LOGIC_EXPORT void DiLogic(Hypodermic::ContainerBuilder& builder, const std::shared_ptr<Hypodermic::Container>& container);

}
