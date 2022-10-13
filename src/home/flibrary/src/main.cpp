#include <QApplication>

#include "controllers/GuiController.h"

using namespace HomeCompa::Flibrary;

int main(int argc, char * argv[])
{
	QApplication app(argc, argv);
	const GuiController guiController;
	return QApplication::exec();
}
