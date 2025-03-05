#include <QCoreApplication>
#include <QStandardPaths>
#include <QTranslator>

#include <Hypodermic/Hypodermic.h>
#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>

#include "interface/IServer.h"
#include "interface/constants/ProductConstant.h"

#include "logging/init.h"
#include "util/ISettings.h"
#include "util/SortString.h"
#include "util/localization.h"
#include "util/xml/Initializer.h"

#include "di_app.h"
#include "log.h"

#include "config/version.h"

using namespace HomeCompa;
using namespace Opds;

namespace
{
constexpr auto APP_ID = "opds";

int run(int argc, char* argv[])
{
	const QCoreApplication app(argc, argv);
	QCoreApplication::setApplicationName(APP_ID);
	QCoreApplication::setApplicationVersion(PRODUCT_VERSION);
	Util::XMLPlatformInitializer xmlPlatformInitializer;

	while (true)
	{
		std::shared_ptr<Hypodermic::Container> container;
		{
			Hypodermic::ContainerBuilder builder;
			DiInit(builder, container);
		}

		auto settings = container->resolve<ISettings>();
		Util::QStringWrapper::SetLocale(Loc::GetLocale(*settings));
		const auto translators = Loc::LoadLocales(*settings); //-V808
		const auto server = container->resolve<IServer>();

		if (const auto code = QCoreApplication::exec(); code != Flibrary::Constant::RESTART_APP)
		{
			PLOGI << "App finished with " << code;
			return code;
		}

		PLOGI << "App restarted";
	}
}

} // namespace

int main(const int argc, char* argv[])
{
	Log::LoggingInitializer logging(QString("%1/%2.%3.log").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), COMPANY_ID, APP_ID).toStdWString());
	PLOGI << QString("%1 started").arg(APP_ID);

	try
	{
		return run(argc, argv);
	}
	catch (const std::exception& ex)
	{
		PLOGE << ex.what();
	}
	catch (...)
	{
		PLOGE << "Unknown error";
	}

	return 1;
}
