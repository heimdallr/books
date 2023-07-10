#pragma once

#include <memory>

#include "logicLib.h"

namespace Hypodermic
{
class Container;
class ContainerBuilder;
}

namespace HomeCompa::Flibrary {

LOGIC_API void di_logic(Hypodermic::ContainerBuilder & builder, const std::shared_ptr<Hypodermic::Container> & container);

}
