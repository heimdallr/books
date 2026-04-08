#pragma once

#include <QPointer>

#include "interface/IParentWidgetProvider.h"

namespace HomeCompa::Util
{

class ParentWidgetProvider final : public IParentWidgetProvider
{
private: // IParentWidgetProvider
	void     SetWidget(QWidget* widget) override;
	QWidget* GetWidget(QWidget* parentWidget) const override;

private:
	QPointer<QWidget> m_widget;
};

}
