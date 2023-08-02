#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/IAnnotationController.h"

namespace HomeCompa::Flibrary {

class AnnotationController final : virtual public IAnnotationController
{
	NON_COPY_MOVABLE(AnnotationController)

public:
	explicit AnnotationController(std::shared_ptr<class ILogicFactory> logicFactory);
	~AnnotationController() override;

private: // IAnnotationController
	void SetCurrentBookId(QString bookId) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
