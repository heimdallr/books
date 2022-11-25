#include <QApplication>
#include <QIcon>
#include <QStandardPaths>

#include <plog/Appenders/RollingFileAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Log.h>

#include "controllers/GuiController.h"
#include "constants/ProductConstant.h"

#include "plog/LogAppender.h"

using namespace HomeCompa;
using namespace Flibrary;

int main(int argc, char * argv[])
{
	try
	{
		plog::RollingFileAppender<plog::TxtFormatter> rollingFileAppender(QString("%1/%2_%3.log").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), Constant::COMPANY_ID, Constant::PRODUCT_ID).toStdWString().data());
		Log::LogAppender logAppender(&rollingFileAppender);

		PLOGI << "App started";

		while (true)
		{
			QApplication app(argc, argv);
			QApplication::setWindowIcon(QIcon(":/icons/main.png"));

			GuiController guiController;
			app.installEventFilter(&guiController);
			guiController.Start();

			if (const auto code = QApplication::exec(); code != 1234)
			{
				PLOGI << "App finished with " << code;
				return code;
			}

			PLOGI << "App restarted";
		}
	}
	catch(const std::exception & ex)
	{
		PLOGF << "App failed with " << ex.what();
	}
	catch(...)
	{
		PLOGF << "App failed with unknown error";
	}
}
