#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IAnnotationController.h"
#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/IJokeRequester.h"
#include "interface/logic/ILogicFactory.h"

namespace HomeCompa::Flibrary
{

class AnnotationController final : virtual public IAnnotationController
{
	NON_COPY_MOVABLE(AnnotationController)

public:
	AnnotationController(const std::shared_ptr<const ILogicFactory>& logicFactory,
	                     std::shared_ptr<const ICollectionProvider> collectionProvider,
	                     std::shared_ptr<IDatabaseUser> databaseUser,
	                     std::shared_ptr<IJokeRequester> jokeRequester);
	~AnnotationController() override;

private: // IAnnotationController
	void SetCurrentBookId(QString bookId, bool extractNow) override;
	QString CreateAnnotation(const IDataProvider& dataProvider, const IStrategy& strategy) const override;
	void ShowJokes(bool value) override;
	void ShowReviews(bool value) override;

	void RegisterObserver(IObserver* observer) override;
	void UnregisterObserver(IObserver* observer) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
