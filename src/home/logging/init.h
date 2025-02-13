#pragma once

#include <filesystem>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "export/logging.h"

namespace HomeCompa::Log
{

class LOGGING_EXPORT LoggingInitializer final
{
	NON_COPY_MOVABLE(LoggingInitializer)

public:
	explicit LoggingInitializer(const std::filesystem::path& path);
	~LoggingInitializer();

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
