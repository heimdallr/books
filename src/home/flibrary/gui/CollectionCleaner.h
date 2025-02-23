#pragma once

#include <QDialog>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

namespace Ui
{
class CollectionCleaner;
};

namespace HomeCompa
{
class ISettings;
}

namespace HomeCompa::Util
{
class IUiFactory;
}

namespace HomeCompa::Flibrary
{

class CollectionCleaner final : public QDialog
{
	NON_COPY_MOVABLE(CollectionCleaner)

public:
	CollectionCleaner(std::shared_ptr<const Util::IUiFactory> uiFactory,
	                  std::shared_ptr<const class IReaderController> readerController,
	                  std::shared_ptr<const class ICollectionCleaner> collectionCleaner,
	                  std::shared_ptr<ISettings> settings,
	                  std::shared_ptr<class IGenreModel> genreModel,
	                  std::shared_ptr<class ILanguageModel> languageModel,
	                  std::shared_ptr<class ScrollBarController> scrollBarControllerGenre,
	                  std::shared_ptr<ScrollBarController> scrollBarControllerLanguage,
	                  QWidget* parent = nullptr);
	~CollectionCleaner() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
