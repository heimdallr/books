#pragma once

#include "fnd/NonCopyMovable.h"

#include "plogLib.h"

namespace plog {
class PLOG_LINKAGE IAppender;
}

namespace HomeCompa::Log {

class PLOG_API LogAppender
{
	NON_COPY_MOVABLE(LogAppender)

public:
	explicit LogAppender(plog::IAppender * appender);
	~LogAppender();

private:
	plog::IAppender * m_appender;
};

}
