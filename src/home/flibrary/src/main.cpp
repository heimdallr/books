#include <QApplication>
#include <QIcon>

#include "plog/Log.h"

#include "controllers/GuiController.h"

using namespace HomeCompa::Flibrary;

int main(int argc, char * argv[])
{
	try
	{
		PLOGI << "App started";

		while (true)
		{
			QApplication app(argc, argv);
			QApplication::setWindowIcon(QIcon(":/icons/main.png"));

			GuiController guiController;
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
