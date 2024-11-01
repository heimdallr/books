#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "export/Util.h"

namespace HomeCompa::Util {

class UTIL_EXPORT XMLPlatformInitializer
{
	NON_COPY_MOVABLE(XMLPlatformInitializer)

public:
	class Impl;

public:
	XMLPlatformInitializer();
	~XMLPlatformInitializer();

private:
	PropagateConstPtr<Impl, std::shared_ptr> m_impl;
};

}
