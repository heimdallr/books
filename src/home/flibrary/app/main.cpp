#include <QApplication>
#include <QMainWindow>
#include <QStandardPaths>

#include <Hypodermic/Hypodermic.h>
#include <plog/Log.h>

#include "interface/constants/ProductConstant.h"
#include "interface/logic/ILogicFactory.h"

#include "logging/init.h"

#include "di_app.h"

#include "Configuration.h"
using namespace HomeCompa;
using namespace Flibrary;

int main(int argc, char * argv[])
{
	Log::LoggingInitializer logging(QString("%1/%2.%3.log").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), Constant::COMPANY_ID, Constant::PRODUCT_ID).toStdWString());
	PLOGI << "App started";
	PLOGI << "Commit hash: " << GIT_HASH;

	try
	{
		while (true)
		{
			QApplication app(argc, argv);
			PLOGD << "QApplication created";

			QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

			std::shared_ptr<Hypodermic::Container> container;
			Hypodermic::ContainerBuilder builder;
			di_init(builder, container).swap(container);
			PLOGD << "DI-container created";

			const auto logicFactory = container->resolve<ILogicFactory>();

			const auto mainWindow = container->resolve<QMainWindow>();
			mainWindow->show();

//			GuiController guiController;
//			app.installEventFilter(&guiController);

//			guiController.Start();

			if (const auto code = QApplication::exec(); code != 1234)
			{
				PLOGI << "App finished with " << code;
				return code;
			}

			PLOGI << "App restarted";
		}
	}
	catch (const std::exception & ex)
	{
		PLOGF << "App failed with " << ex.what();
	}
	catch (...)
	{
		PLOGF << "App failed with unknown error";
	}
}
