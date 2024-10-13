#pragma once

#include <QPointer>
#include <QWidget>

namespace HomeCompa {

class IParentWidgetProvider
{
public:
	virtual ~IParentWidgetProvider() = default;

public:
	virtual void SetWidget(QPointer<QWidget> widget) = 0;
	virtual QWidget * GetWidget() const = 0;
};

}
