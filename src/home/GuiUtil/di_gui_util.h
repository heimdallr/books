#pragma once

#include <memory>

#include "export/GuiUtil.h"

namespace Hypodermic
{
class Container;
class ContainerBuilder;
}

namespace HomeCompa::Util
{

GUIUTIL_EXPORT void DiGuiUtil(Hypodermic::ContainerBuilder& builder, const std::shared_ptr<Hypodermic::Container>& container);

}
