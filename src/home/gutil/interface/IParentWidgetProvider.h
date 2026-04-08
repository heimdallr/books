#pragma once

class QWidget;

namespace HomeCompa
{

class IParentWidgetProvider // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IParentWidgetProvider() = default;

public:
	virtual void     SetWidget(QWidget* widget)                       = 0;
	virtual QWidget* GetWidget(QWidget* parentWidget = nullptr) const = 0;
};

}
