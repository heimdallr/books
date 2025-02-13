#include "ParentWidgetProvider.h"

using namespace HomeCompa::Util;

void ParentWidgetProvider::SetWidget(QPointer<QWidget> widget)
{
	m_widget = std::move(widget);
}

QWidget* ParentWidgetProvider::GetWidget() const
{
	return m_widget;
}
