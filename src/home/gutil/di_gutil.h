#pragma once

#include <memory>

#include "export/gutil.h"

namespace Hypodermic
{

class Container;
class ContainerBuilder;

}

namespace HomeCompa::Util
{

GUTIL_EXPORT void DiGUtil(Hypodermic::ContainerBuilder& builder, const std::shared_ptr<Hypodermic::Container>& container);

}
