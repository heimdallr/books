#include "init.h"

#include <plog/Appenders/RollingFileAppender.h>
#include <plog/Formatters/TxtFormatter.h>

#include "LogAppender.h"
#include "QtLoHandler.h"
#include "log.h"

namespace HomeCompa::Log
{

struct LoggingInitializer::Impl
{
	plog::RollingFileAppender<plog::TxtFormatter> rollingFileAppender;
	LogAppender                                   logAppender;
	QtLogHandler                                  qtLogHandler;

	explicit Impl(const std::filesystem::path& path)
		: rollingFileAppender(path.string().data(), 1024 * 1024 * 1024, 10)
		, logAppender(&rollingFileAppender)
	{
	}
};

LoggingInitializer::LoggingInitializer(const std::filesystem::path& path)
	: m_impl(path)
{
}

LoggingInitializer::~LoggingInitializer() = default;

}
