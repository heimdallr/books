#pragma once

#include <QDialog>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IModelProvider.h"
#include "interface/ui/dialogs/ISettingsDialog.h"

#include "gutil/interface/IParentWidgetProvider.h"
#include "util/ISettings.h"

#include "ItemViewToolTipper.h"
#include "ScrollBarController.h"

namespace HomeCompa::Flibrary
{

class SettingsDialog final
	: public QDialog
	, virtual public ISettingsDialog
{
	NON_COPY_MOVABLE(SettingsDialog)

public:
	SettingsDialog(
		const std::shared_ptr<IParentWidgetProvider>& parentWidgetProvider,
		const std::shared_ptr<IModelProvider>&        modelProvider,
		std::shared_ptr<ISettings>                    settings,
		std::shared_ptr<ItemViewToolTipper>           itemViewToolTipper,
		std::shared_ptr<ScrollBarController>          scrollBarController,
		QWidget*                                      parent = nullptr
	);
	~SettingsDialog() override;

private: // ISettingsDialog
	int Exec() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
