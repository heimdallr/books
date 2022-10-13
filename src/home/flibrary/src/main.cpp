#include <QApplication>

#include "controllers/GuiController.h"

using namespace HomeCompa::Flibrary;

int main(int argc, char * argv[])
{
	if (argc < 2)
		return 1;

	QApplication app(argc, argv);

	GuiController guiController(argv[1]);
	guiController.Start();

	return QApplication::exec();
}
