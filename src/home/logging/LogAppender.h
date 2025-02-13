#pragma once

#include "fnd/NonCopyMovable.h"

#include "export/logging.h"

namespace plog
{
class IAppender;
}

namespace HomeCompa::Log
{

class LOGGING_EXPORT LogAppender
{
	NON_COPY_MOVABLE(LogAppender)

public:
	explicit LogAppender(plog::IAppender* appender);
	~LogAppender();

private:
	plog::IAppender* m_appender;
};

}
