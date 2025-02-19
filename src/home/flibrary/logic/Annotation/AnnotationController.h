#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IAnnotationController.h"

namespace HomeCompa::Flibrary
{

class AnnotationController final : virtual public IAnnotationController
{
	NON_COPY_MOVABLE(AnnotationController)

public:
	AnnotationController(const std::shared_ptr<const class ILogicFactory>& logicFactory, std::shared_ptr<class IDatabaseUser> databaseUser, std::shared_ptr<class IJokeRequester> jokeRequester);
	~AnnotationController() override;

private: // IAnnotationController
	void SetCurrentBookId(QString bookId, bool extractNow) override;
	QString CreateAnnotation(const IDataProvider& dataProvider) const override;

	void RegisterObserver(IObserver* observer) override;
	void UnregisterObserver(IObserver* observer) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
