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

class GenreFilterDialog final : public QDialog
{
	NON_COPY_MOVABLE(GenreFilterDialog)

public:
	GenreFilterDialog(const std::shared_ptr<const IParentWidgetProvider>& parentWidgetProvider,
	                  const std::shared_ptr<const IGenreFilterProvider>& genreFilterProvider,
	                  std::shared_ptr<ISettings> settings,
	                  std::shared_ptr<IGenreFilterController> genreFilterController,
	                  std::shared_ptr<IGenreModel> genreModel,
	                  std::shared_ptr<ScrollBarController> scrollBarController,
	                  QWidget* parent = nullptr);
	~GenreFilterDialog() override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
