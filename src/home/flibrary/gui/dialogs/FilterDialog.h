#pragma once

#include <QDialog>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IFilterProvider.h"

#include "gutil/interface/IParentWidgetProvider.h"
#include "util/ISettings.h"

#include "ScrollBarController.h"

namespace HomeCompa::Flibrary
{

class FilterDialog final : public QDialog
{
	NON_COPY_MOVABLE(FilterDialog)

public:
	FilterDialog(const std::shared_ptr<const IParentWidgetProvider>& parentWidgetProvider,
	             std::shared_ptr<ISettings> settings,
	             std::shared_ptr<IFilterController> languageFilterController,
	             std::shared_ptr<ScrollBarController> scrollBarController,
	             QWidget* parent = nullptr);
	~FilterDialog() override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
