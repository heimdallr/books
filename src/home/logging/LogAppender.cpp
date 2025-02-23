#include "LogAppender.h"

#include <plog/Appenders/DynamicAppender.h>
#include <plog/Appenders/IAppender.h>
#include <plog/Init.h>

namespace HomeCompa::Log
{

namespace
{
std::unique_ptr<plog::DynamicAppender> g_dynamicAppender;
}

LogAppender::LogAppender(plog::IAppender* appender)
	: m_appender(appender)
{
	if (!g_dynamicAppender)
	{
		g_dynamicAppender = std::make_unique<plog::DynamicAppender>();
		init(plog::Severity::verbose, g_dynamicAppender.get());
	}

	g_dynamicAppender->addAppender(m_appender);
}

LogAppender::~LogAppender()
{
	g_dynamicAppender->removeAppender(m_appender);
}

} // namespace HomeCompa::Log
