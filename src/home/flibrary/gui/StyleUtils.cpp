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

void SetHeaderViewStyleImpl(QWidget & widget)
{
	const auto style = QString("QHeaderView::section { background-color: %1; border: 1px solid %2; }")
		.arg(Color(widget.palette().color(QPalette::Base)))
		.arg(Color(widget.palette().color(QPalette::Disabled, QPalette::Text)))
		;
	widget.setStyleSheet(style);
}

void SetLineEditWithErrorStyleStub(QWidget &)
{
}

void SetLineEditWithErrorStyleImpl(QWidget & widget)
{
	constexpr auto style = R"(QLineEdit[errorTag="true"] { border: 1px solid #FF0000; } QLineEdit[errorTag="false"] { border: 1px solid #000000; })";
	widget.setStyleSheet(style);
}

using SetWidgetStyleFunctor = void (*)(QWidget &);
SetWidgetStyleFunctor g_setHeaderViewStyleFunctor = &SetHeaderViewStyleImpl;
SetWidgetStyleFunctor g_setLineEditWithErrorStyleFunctor = &SetLineEditWithErrorStyleImpl;

}


void SetHeaderViewStyle(QWidget & widget)
{
	g_setHeaderViewStyleFunctor(widget);
}

void SetLineEditWithErrorStyle(QWidget & widget)
{
	g_setLineEditWithErrorStyleFunctor(widget);
}

void EnableStyleUtils(const bool enabled)
{
	g_setHeaderViewStyleFunctor = enabled ? &SetHeaderViewStyleImpl : &SetHeaderViewStyleStub;
	g_setLineEditWithErrorStyleFunctor = enabled ? &SetLineEditWithErrorStyleImpl : &SetLineEditWithErrorStyleStub;
}

}
