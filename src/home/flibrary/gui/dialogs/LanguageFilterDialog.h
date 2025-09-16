#pragma once

#include <QDialog>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IBooksFilterProvider.h"
#include "interface/logic/IModel.h"
#include "interface/ui/IUiFactory.h"

#include "gutil/interface/IParentWidgetProvider.h"
#include "util/ISettings.h"

#include "ScrollBarController.h"

namespace HomeCompa::Flibrary
{

class LanguageFilterDialog final : public QDialog
{
	NON_COPY_MOVABLE(LanguageFilterDialog)

public:
	LanguageFilterDialog(const std::shared_ptr<const IParentWidgetProvider>& parentWidgetProvider,
	                     const std::shared_ptr<const ILanguageFilterProvider>& languageFilterProvider,
	                     std::shared_ptr<ISettings> settings,
	                     std::shared_ptr<ILanguageFilterController> languageFilterController,
	                     std::shared_ptr<ILanguageModel> languageModel,
	                     std::shared_ptr<ScrollBarController> scrollBarController,
	                     QWidget* parent = nullptr);
	~LanguageFilterDialog() override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
