#pragma once

#include <QDialog>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IOpdsController.h"

#include "GuiUtil/interface/IParentWidgetProvider.h"
#include "util/Settings.h"

namespace HomeCompa::Flibrary
{

class OpdsDialog : public QDialog
{
	NON_COPY_MOVABLE(OpdsDialog)

public:
	OpdsDialog(const std::shared_ptr<IParentWidgetProvider>& parentWidgetProvider, std::shared_ptr<ISettings> settings, std::shared_ptr<IOpdsController> opdsController, QWidget* parent = nullptr);
	~OpdsDialog() override;

private: // QDialog
	int exec() override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
