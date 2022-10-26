#include <QApplication>
#include <QIcon>

#include "controllers/GuiController.h"

using namespace HomeCompa::Flibrary;

int main(int argc, char * argv[])
{
	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QApplication app(argc, argv);
	app.setWindowIcon(QIcon(":/icons/main.png"));

	GuiController guiController(argc > 1 ? argv[1] : "");
	guiController.Start();

	return QApplication::exec();
}
