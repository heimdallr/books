#pragma once

#include <memory>

namespace Hypodermic {

class Container;
class ContainerBuilder;

} // namespace Hypodermic

namespace HomeCompa::Opds {

void DiInit(Hypodermic::ContainerBuilder& builder, std::shared_ptr<Hypodermic::Container>& container);

}
