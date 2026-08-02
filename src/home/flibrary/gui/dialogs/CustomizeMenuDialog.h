#pragma once

#include <QDialog>

#include "fnd/memory.h"

#include "interface/logic/IMenuCustomizer.h"
#include "interface/logic/IModelProvider.h"

#include "gutil/interface/IParentWidgetProvider.h"
#include "gutil/interface/IUiFactory.h"
#include "settings/ISettings.h"
#include "utilgui/ItemViewToolTipper.h"
#include "utilgui/ScrollBarController.h"

namespace HomeCompa::Flibrary
{

class CustomizeMenuDialog final : public QDialog
{
	NON_COPY_MOVABLE(CustomizeMenuDialog)

public:
	CustomizeMenuDialog(
		const std::shared_ptr<IParentWidgetProvider>& parentWidgetProvider,
		const std::shared_ptr<IModelProvider>&        modelProvider,
		std::shared_ptr<const Util::IUiFactory>       uiFactory,
		std::shared_ptr<ISettings>                    settings,
		std::shared_ptr<IMenuCustomizer>              menuCustomizer,
		std::shared_ptr<Util::ItemViewToolTipper>     itemViewToolTipper,
		std::shared_ptr<Util::ScrollBarController>    scrollBarController,
		QWidget*                                      parent = nullptr
	);
	~CustomizeMenuDialog() override;

private:
	void showEvent(QShowEvent* event) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
