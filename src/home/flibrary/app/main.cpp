#include <QApplication>
#include <QFileInfo>
#include <QPalette>
#include <QStandardPaths>
#include <QStyleFactory>

#include <Hypodermic/Hypodermic.h>

#include "fnd/FindPair.h"

#include "interface/constants/ProductConstant.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/IOpdsController.h"
#include "interface/logic/ITaskQueue.h"
#include "interface/ui/IMainWindow.h"
#include "interface/ui/IStyleApplierFactory.h"

#include "logging/init.h"
#include "logic/model/LogModel.h"
#include "util/ISettings.h"
#include "util/xml/Initializer.h"
#include "version/AppVersion.h"

#include "di_app.h"
#include "log.h"

#include "config/git_hash.h"
#include "config/version.h"

using namespace HomeCompa;
using namespace Flibrary;

int main(int argc, char* argv[])
{
	Log::LoggingInitializer logging(QString("%1/%2.%3.log").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), COMPANY_ID, PRODUCT_ID).toStdWString());
	LogModelAppender logModelAppender;

	PLOGI << "App started";
	PLOGI << "Version: " << GetApplicationVersion();
	PLOGI << "Commit hash: " << GIT_HASH;

	try
	{
		while (true)
		{
			QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

			QApplication app(argc, argv);
			QCoreApplication::setApplicationName(PRODUCT_ID);
			QCoreApplication::setApplicationVersion(PRODUCT_VERSION);
			Util::XMLPlatformInitializer xmlPlatformInitializer;

			PLOGD << "QApplication created";

			std::shared_ptr<Hypodermic::Container> container;
			{
				Hypodermic::ContainerBuilder builder;
				DiInit(builder, container);
			}
			PLOGD << "DI-container created";

			const auto settings = container->resolve<ISettings>();
			auto styleApplierFactory = container->resolve<IStyleApplierFactory>();
			const auto themeLib = styleApplierFactory->CreateThemeApplier()->Set(app);
			const auto colorSchemeLib = styleApplierFactory->CreateStyleApplier(IStyleApplier::Type::ColorScheme)->Set(app);
			styleApplierFactory.reset();

			container->resolve<ITaskQueue>()->Execute();
			const auto mainWindow = container->resolve<IMainWindow>();
			container->resolve<IDatabaseUser>()->EnableApplicationCursorChange(true);
			mainWindow->Show();

			if (const auto code = QApplication::exec(); code != Constant::RESTART_APP)
			{
				PLOGI << "App finished with " << code;
				return code;
			}

			container->resolve<IOpdsController>()->Restart();

			PLOGI << "App restarted";
		}
	}
	catch (const std::exception& ex)
	{
		PLOGF << "App failed with " << ex.what();
	}
	catch (...)
	{
		PLOGF << "App failed with unknown error";
	}
}
