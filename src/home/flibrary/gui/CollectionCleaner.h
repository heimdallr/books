#pragma once

#include <QDialog>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/ICollectionCleaner.h"
#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IModel.h"
#include "interface/logic/IReaderController.h"

#include "GuiUtil/interface/IUiFactory.h"
#include "util/ISettings.h"

#include "ScrollBarController.h"

namespace Ui
{
class CollectionCleaner;
};

namespace HomeCompa::Flibrary
{

class CollectionCleaner final : public QDialog
{
	NON_COPY_MOVABLE(CollectionCleaner)

public:
	CollectionCleaner(std::shared_ptr<const Util::IUiFactory> uiFactory,
	                  std::shared_ptr<const IReaderController> readerController,
	                  std::shared_ptr<const ICollectionCleaner> collectionCleaner,
	                  std::shared_ptr<ISettings> settings,
	                  std::shared_ptr<IGenreModel> genreModel,
	                  std::shared_ptr<ILanguageModel> languageModel,
	                  std::shared_ptr<ScrollBarController> scrollBarControllerGenre,
	                  std::shared_ptr<ScrollBarController> scrollBarControllerLanguage,
	                  QWidget* parent = nullptr);
	~CollectionCleaner() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
