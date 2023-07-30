#pragma once

#include <memory>

#include "FlUILib.h"

namespace Hypodermic
{
class Container;
class ContainerBuilder;
}

namespace HomeCompa::Flibrary {

FLUI_API void DiUi(Hypodermic::ContainerBuilder & builder, const std::shared_ptr<Hypodermic::Container> & container);

}
