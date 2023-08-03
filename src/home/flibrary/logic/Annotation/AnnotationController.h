#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/IAnnotationController.h"

namespace HomeCompa::Flibrary {

class AnnotationController final : virtual public IAnnotationController
{
	NON_COPY_MOVABLE(AnnotationController)

public:
	AnnotationController(const std::shared_ptr<class ILogicFactory> & logicFactory
		, std::shared_ptr<class DatabaseUser> databaseUser
	);
	~AnnotationController() override;

private: // IAnnotationController
	void SetCurrentBookId(QString bookId) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
