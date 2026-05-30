#pragma once

#include <QDialog>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IModelProvider.h"
#include "interface/ui/IUiFactory.h"

#include "gutil/interface/IParentWidgetProvider.h"
#include "settings/ISettings.h"

#include "ComboBoxDelegate.h"
#include "ItemViewToolTipper.h"
#include "ScriptNameDelegate.h"
#include "ScrollBarController.h"

namespace HomeCompa::Flibrary
{

class ScriptDialog final : public QDialog
{
	NON_COPY_MOVABLE(ScriptDialog)

public:
	ScriptDialog(
		const std::shared_ptr<IParentWidgetProvider>& parentWidgetProvider,
		const std::shared_ptr<const IModelProvider>&  modelProvider,
		std::shared_ptr<const IUiFactory>             uiFactory,
		std::shared_ptr<ISettings>                    settings,
		std::shared_ptr<ScriptComboBoxDelegate>       scriptTypeDelegate,
		std::shared_ptr<ScriptNameDelegate>           scriptNameLineEditDelegate,
		std::shared_ptr<ItemViewToolTipper>           scriptItemViewToolTipper,
		std::shared_ptr<ItemViewToolTipper>           commandItemViewToolTipper,
		std::shared_ptr<ScrollBarController>          scriptScrollBarController,
		std::shared_ptr<ScrollBarController>          commandScrollBarController

	);
	~ScriptDialog() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
