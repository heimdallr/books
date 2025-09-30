#pragma once

#include <QDialog>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IDataProvider.h"
#include "interface/logic/IFilterController.h"
#include "interface/logic/IModelProvider.h"

#include "gutil/interface/IParentWidgetProvider.h"
#include "util/ISettings.h"

#include "ItemViewToolTipper.h"
#include "ScrollBarController.h"

namespace HomeCompa::Flibrary
{

class FilterSettingsDialog final : public QDialog
{
	NON_COPY_MOVABLE(FilterSettingsDialog)

public:
	FilterSettingsDialog(
		const std::shared_ptr<const IParentWidgetProvider>& parentWidgetProvider,
		std::shared_ptr<const IModelProvider>               modelProvider,
		std::shared_ptr<ISettings>                          settings,
		std::shared_ptr<IFilterController>                  filterController,
		std::shared_ptr<IFilterDataProvider>                dataProvider,
		std::shared_ptr<ItemViewToolTipper>                 itemViewToolTipper,
		std::shared_ptr<ScrollBarController>                scrollBarController,
		QWidget*                                            parent = nullptr
	);
	~FilterSettingsDialog() override;

private:
	class Impl;
	PropagateConstPtr<Impl, std::shared_ptr> m_impl;
};

}
