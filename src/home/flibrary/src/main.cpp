#include <QApplication>
#include <QIcon>

#include "controllers/GuiController.h"

using namespace HomeCompa::Flibrary;

int main(int argc, char * argv[])
{
	if (argc < 2)
		return 1;

	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QGuiApplication app(argc, argv);
	app.setWindowIcon(QIcon(":/icons/main.png"));

	GuiController guiController(argv[1]);
	guiController.Start();

	return QApplication::exec();
}
