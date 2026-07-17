#pragma once

#include <QWidget>

#include "fnd/memory.h"

#include "interface/ui/IUiFactory.h"

namespace HomeCompa::Flibrary
{

class ChangeSizeDialog final : public QWidget
{
public:
	ChangeSizeDialog(int current, int minimum, int maximum, IUiFactory::IChangeSizeWidgetObserver* observer, QWidget* parent = nullptr);
	~ChangeSizeDialog() override;

private:
	class Impl;
	PropagateConstPtr<Impl>   m_impl;
};

}
