#pragma once

#include <filesystem>

#include "fnd/memory.h"

#include "loggingLib.h"

namespace HomeCompa::Log {

class LOGGING_API LoggingInitializer
{
public:
	explicit LoggingInitializer(const std::filesystem::path & path);
	~LoggingInitializer();

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
