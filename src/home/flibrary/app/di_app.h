#pragma once

#include <memory>

namespace Hypodermic
{
class Container;
class ContainerBuilder;
}

namespace HomeCompa::Flibrary {

std::shared_ptr<Hypodermic::Container> DiInit(Hypodermic::ContainerBuilder & builder, const std::shared_ptr<Hypodermic::Container> & container);

}
