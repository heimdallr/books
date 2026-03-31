#pragma once

#include <QDialog>

#include "fnd/memory.h"

#include "interface/logic/IHotkeyManager.h"
#include "interface/logic/IModelProvider.h"

#include "gutil/interface/IParentWidgetProvider.h"
#include "util/ISettings.h"

#include "ItemViewToolTipper.h"
#include "ScrollBarController.h"

namespace HomeCompa::Flibrary
{

class HotkeyDialog final : public QDialog
{
	NON_COPY_MOVABLE(HotkeyDialog)

public:
	HotkeyDialog(
		const std::shared_ptr<IParentWidgetProvider>& parentWidgetProvider,
		const std::shared_ptr<IModelProvider>&        modelProvider,
		std::shared_ptr<ISettings>                    settings,
		std::shared_ptr<IHotkeyManager>               hotkeyManager,
		std::shared_ptr<ItemViewToolTipper>           itemViewToolTipper,
		std::shared_ptr<ScrollBarController>          scrollBarController,
		QWidget*                                      parent = nullptr
	);
	~HotkeyDialog() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
