#pragma once

#include <memory>

#include "export/gui.h"

namespace Hypodermic
{

class Container;
class ContainerBuilder;

}

namespace HomeCompa::Flibrary
{

GUI_EXPORT void DiUi(Hypodermic::ContainerBuilder& builder, const std::shared_ptr<Hypodermic::Container>& container);

}
