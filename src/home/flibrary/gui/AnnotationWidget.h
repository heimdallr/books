#pragma once

#include <QWidget>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

namespace HomeCompa
{
class ISettings;
}

namespace HomeCompa::Flibrary
{

class AnnotationWidget final : public QWidget
{
	NON_COPY_MOVABLE(AnnotationWidget)

public:
	AnnotationWidget(const std::shared_ptr<const class IModelProvider>& modelProvider,
	                 const std::shared_ptr<const class ILogicFactory>& logicFactory,
	                 const std::shared_ptr<class ICollectionController>& collectionController,
	                 std::shared_ptr<const class IReaderController> readerController,
	                 std::shared_ptr<ISettings> settings,
	                 std::shared_ptr<class IAnnotationController> annotationController,
	                 std::shared_ptr<class IUiFactory> uiFactory,
	                 std::shared_ptr<class IBooksExtractorProgressController> progressController,
	                 std::shared_ptr<class ItemViewToolTipper> itemViewToolTipperContent,
	                 std::shared_ptr<class ScrollBarController> scrollBarControllerContent,
	                 std::shared_ptr<ScrollBarController> scrollBarControllerAnnotation,
	                 QWidget* parent = nullptr);
	~AnnotationWidget() override;

public:
	void ShowContent(bool value);
	void ShowCover(bool value);
	void ShowCoverButtons(bool value);

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
