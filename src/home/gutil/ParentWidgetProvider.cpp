#include "ParentWidgetProvider.h"

#include <QWidget>

using namespace HomeCompa::Util;

void ParentWidgetProvider::SetWidget(QWidget* widget)
{
	m_widget = widget;
}

QWidget* ParentWidgetProvider::GetWidget(QWidget* parentWidget) const
{
	return m_widget ? m_widget.data() : parentWidget;
}
