#pragma once

#include <QWidget>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IAnnotationController.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IModelProvider.h"
#include "interface/logic/IProgressController.h"
#include "interface/logic/IReaderController.h"
#include "interface/ui/IUiFactory.h"

#include "util/ISettings.h"

#include "ItemViewToolTipper.h"
#include "ScrollBarController.h"

namespace HomeCompa::Flibrary
{

class AnnotationWidget final : public QWidget
{
	Q_OBJECT
	NON_COPY_MOVABLE(AnnotationWidget)

public:
	AnnotationWidget(const std::shared_ptr<const IModelProvider>& modelProvider,
	                 const std::shared_ptr<const ILogicFactory>& logicFactory,
	                 const std::shared_ptr<ICollectionController>& collectionController,
	                 std::shared_ptr<const IReaderController> readerController,
	                 std::shared_ptr<ISettings> settings,
	                 std::shared_ptr<IAnnotationController> annotationController,
	                 std::shared_ptr<IUiFactory> uiFactory,
	                 std::shared_ptr<IBooksExtractorProgressController> progressController,
	                 std::shared_ptr<ItemViewToolTipper> itemViewToolTipperContent,
	                 std::shared_ptr<ScrollBarController> scrollBarControllerContent,
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
