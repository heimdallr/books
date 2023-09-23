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
	AnnotationWidget(std::shared_ptr<ISettings> settings
		, std::shared_ptr<class IAnnotationController> annotationController
		, std::shared_ptr<class IModelProvider> modelProvider
		, std::shared_ptr<class IUiFactory> uiFactory
		, std::shared_ptr<class ICollectionController> collectionController
		, const std::shared_ptr<class ILogicFactory> & logicFactory
		, QWidget * parent = nullptr
	);
	~AnnotationWidget() override;

public:
	void ShowContent(bool value);
	void ShowCover(bool value);

private:
	void resizeEvent(QResizeEvent* event) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
