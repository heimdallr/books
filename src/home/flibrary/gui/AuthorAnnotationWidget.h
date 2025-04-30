#pragma once

#include <QFrame>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IAuthorAnnotationController.h"

namespace HomeCompa::Flibrary
{

class AuthorAnnotationWidget : public QFrame
{
	Q_OBJECT
	NON_COPY_MOVABLE(AuthorAnnotationWidget)

public:
	AuthorAnnotationWidget(std::shared_ptr<IAuthorAnnotationController> annotationController, QWidget* parent = nullptr);
	~AuthorAnnotationWidget() override;

public:
	void Show(bool value);

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
