#include <QQmlApplicationEngine>

#include "GuiController.h"

namespace HomeCompa::Flibrary {

class GuiController::Impl
{
public:
	Impl()
	{
		m_qmlEngine.load("qrc:/qml/Main.qml");
	}

	~Impl()
	{
		m_qmlEngine.clearComponentCache();
	}

private:
	QQmlApplicationEngine m_qmlEngine;
};

GuiController::GuiController() = default;
GuiController::~GuiController() = default;

}
