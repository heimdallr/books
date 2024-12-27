#include <QCoreApplication>
#include <QStandardPaths>

#include <Hypodermic/Hypodermic.h>

#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Log.h>

#include "interface/IFactory.h"
#include "interface/IServer.h"

#include "logging/init.h"

#include "di_app.h"

#include "config/version.h"

using namespace HomeCompa;
using namespace Opds;

namespace {
constexpr auto APP_ID = "opds";

int run(int argc, char * argv[])
{
	const QCoreApplication app(argc, argv);
	QCoreApplication::setApplicationName(APP_ID);
	QCoreApplication::setApplicationVersion(PRODUCT_VERSION);
	std::shared_ptr<Hypodermic::Container> container;
	{
		Hypodermic::ContainerBuilder builder;
		DiInit(builder, container);
	}

	const auto factory = container->resolve<IFactory>();
	const auto server = factory->CreateServer();

	return QCoreApplication::exec();
}

}

int main(const int argc, char *argv[])
{
	Log::LoggingInitializer logging(QString("%1/%2.%3.log").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), COMPANY_ID, APP_ID).toStdWString());
	PLOGI << QString("%1 started").arg(APP_ID);

	try
	{
		return run(argc, argv);
	}
	catch(const std::exception & ex)
	{
		PLOGE << ex.what();
	}
	catch(...)
	{
		PLOGE << "Unknown error";
	}

	return 1;
}
