#pragma once

#include <QPointer>
#include <QWidget>

namespace HomeCompa::Flibrary {

class ParentWidgetProvider
{
public:
	void SetWidget(QPointer<QWidget> widget);
	QWidget * GetWidget() const;

private:
	QPointer<QWidget> m_widget;
};

}
