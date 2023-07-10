#pragma once

#include "loggingLib.h"

namespace HomeCompa::Log {

class LOGGING_API LoggingInitializer
{
public:
	explicit LoggingInitializer(const wchar_t * filename);
	~LoggingInitializer();
};

}
