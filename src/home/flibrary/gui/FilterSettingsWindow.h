#pragma once

#include <QDialog>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IFilterProvider.h"

#include "gutil/interface/IParentWidgetProvider.h"
#include "util/ISettings.h"

#include "ScrollBarController.h"
#include "StackedPage.h"

namespace HomeCompa::Flibrary
{

class FilterSettingsWindow final : public StackedPage
{
	NON_COPY_MOVABLE(FilterSettingsWindow)

public:
	FilterSettingsWindow(const std::shared_ptr<const IParentWidgetProvider>& parentWidgetProvider,
	                     std::shared_ptr<ISettings> settings,
	                     std::shared_ptr<IFilterController> languageFilterController,
	                     std::shared_ptr<ScrollBarController> scrollBarController,
	                     QWidget* parent = nullptr);
	~FilterSettingsWindow() override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
