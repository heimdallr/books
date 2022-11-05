#include <QApplication>
#include <QIcon>

#include "controllers/GuiController.h"

using namespace HomeCompa::Flibrary;

int main(int argc, char * argv[])
{
	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

	while(true)
	{
		QApplication app(argc, argv);
		QApplication::setWindowIcon(QIcon(":/icons/main.png"));

		GuiController guiController(argc > 1 ? argv[1] : "");
		guiController.Start();

		if (const auto code = QApplication::exec(); code != 1234)
			return code;
	}
}
