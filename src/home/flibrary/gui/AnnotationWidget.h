#pragma once

#include <QWidget>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Flibrary {

class AnnotationWidget final : public QWidget
{
	NON_COPY_MOVABLE(AnnotationWidget)

public:
	AnnotationWidget(const std::shared_ptr<const class IModelProvider>& modelProvider
		, const std::shared_ptr<const class ILogicFactory>& logicFactory
		, const std::shared_ptr<class ICollectionController>& collectionController
		, std::shared_ptr<const class IReaderController> readerController
		, std::shared_ptr<ISettings> settings
		, std::shared_ptr<class IAnnotationController> annotationController		
		, std::shared_ptr<class IUiFactory> uiFactory
		, std::shared_ptr<class IBooksExtractorProgressController> progressController
		, QWidget * parent = nullptr
	);
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
