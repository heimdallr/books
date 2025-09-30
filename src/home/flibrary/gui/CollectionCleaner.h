#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/ICollectionCleaner.h"
#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDataProvider.h"
#include "interface/logic/IModel.h"
#include "interface/logic/IReaderController.h"
#include "interface/ui/IUiFactory.h"

#include "util/ISettings.h"

#include "ScrollBarController.h"
#include "StackedPage.h"

namespace Ui
{
class CollectionCleaner;
};

namespace HomeCompa::Flibrary
{

class CollectionCleaner final : public StackedPage
{
	NON_COPY_MOVABLE(CollectionCleaner)

public:
	CollectionCleaner(
		const std::shared_ptr<const ICollectionProvider>& collectionProvider,
		std::shared_ptr<const IUiFactory>                 uiFactory,
		std::shared_ptr<const IReaderController>          readerController,
		std::shared_ptr<const ICollectionCleaner>         collectionCleaner,
		std::shared_ptr<const IBookInfoProvider>          dataProvider,
		std::shared_ptr<ISettings>                        settings,
		std::shared_ptr<IGenreModel>                      genreModel,
		std::shared_ptr<ILanguageModel>                   languageModel,
		std::shared_ptr<ScrollBarController>              scrollBarControllerGenre,
		std::shared_ptr<ScrollBarController>              scrollBarControllerLanguage,
		QWidget*                                          parent = nullptr
	);
	~CollectionCleaner() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
