#pragma once

#include "fnd/NonCopyMovable.h"

#include "export/Util.h"

namespace HomeCompa::Util
{

class UTIL_EXPORT XMLPlatformInitializer
{
	NON_COPY_MOVABLE(XMLPlatformInitializer)

public:
	XMLPlatformInitializer();
	~XMLPlatformInitializer();
};

}
