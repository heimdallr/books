#include "StyleUtils.h"

#include <QWidget>

namespace HomeCompa::Flibrary::StyleUtils {

namespace {

QString Color(const QColor & color)
{
	return QString("rgb(%1, %2, %3)").arg(color.red()).arg(color.green()).arg(color.blue());
}

void SetHeaderViewStyleStub(QWidget &)
{
}

void SetHeaderViewStyleImpl(QWidget & view)
{
	const auto style = QString("QHeaderView::section { background-color: %1; border: 1px solid %2; }")
		.arg(Color(view.palette().color(QPalette::Base)))
		.arg(Color(view.palette().color(QPalette::Disabled, QPalette::Text)))
		;
	view.setStyleSheet(style);
}

using SetHeaderViewStyleFunctor = void (*)(QWidget & view);

SetHeaderViewStyleFunctor g_setHeaderViewStyleFunctor = &SetHeaderViewStyleImpl;

}


void SetHeaderViewStyle(QWidget & view)
{
	g_setHeaderViewStyleFunctor(view);
}

void EnableSetHeaderViewStyle(const bool enabled)
{
	g_setHeaderViewStyleFunctor = enabled ? &SetHeaderViewStyleImpl : &SetHeaderViewStyleStub;
}

}
