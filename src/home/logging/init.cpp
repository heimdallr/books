#include "init.h"

#include <plog/Appenders/RollingFileAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Log.h>

#include "LogAppender.h"

namespace HomeCompa::Log {

struct LoggingInitializer::Impl
{
	plog::RollingFileAppender<plog::TxtFormatter> rollingFileAppender;
	LogAppender logAppender;

	Impl(const std::filesystem::path & path)
		: rollingFileAppender(path.string().data(), 1000000, 1000)
		, logAppender(&rollingFileAppender)
	{
	}
};

LoggingInitializer::LoggingInitializer(const std::filesystem::path & path)
	: m_impl(path)
{
}

LoggingInitializer::~LoggingInitializer() = default;

}
