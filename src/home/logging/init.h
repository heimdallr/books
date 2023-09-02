#pragma once

#include <filesystem>

#include "fnd/memory.h"

#include "logging_export.h"

namespace HomeCompa::Log {

class LOGGING_EXPORT LoggingInitializer
{
public:
	explicit LoggingInitializer(const std::filesystem::path & path);
	~LoggingInitializer();

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
