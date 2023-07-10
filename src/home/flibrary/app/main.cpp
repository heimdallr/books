#include <QApplication>
#include <QStandardPaths>

#include <Hypodermic/Hypodermic.h>
#include <plog/Log.h>

#include "constants/ProductConstant.h"
#include "logging/init.h"

#include "Configuration.h"

using namespace HomeCompa;
using namespace Flibrary;

int main(int argc, char * argv[])
{
	try
	{
		Log::LoggingInitializer logging(QString("%1/%2.%3.log").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), Constant::COMPANY_ID, Constant::PRODUCT_ID).toStdWString().data());

		PLOGI << "App started";
		PLOGI << "Commit hash: " << GIT_HASH;

		QApplication app(argc, argv);
		PLOGD << "QApp created";

		while (true)
		{
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
