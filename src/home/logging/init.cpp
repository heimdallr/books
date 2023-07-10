#include "init.h"

#include <plog/Appenders/RollingFileAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Log.h>

#include "LogAppender.h"

namespace HomeCompa::Log {

LoggingInitializer::LoggingInitializer(const wchar_t * filename)
{
	plog::RollingFileAppender<plog::TxtFormatter> rollingFileAppender(filename);
	LogAppender logAppender(&rollingFileAppender);
}

LoggingInitializer::~LoggingInitializer() = default;

}