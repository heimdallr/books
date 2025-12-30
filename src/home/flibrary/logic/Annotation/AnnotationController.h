#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IAnnotationController.h"
#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/IFilterProvider.h"
#include "interface/logic/ILogicFactory.h"

#include "util/ISettings.h"

namespace HomeCompa::Flibrary
{

class AnnotationController final : virtual public IAnnotationController
{
	NON_COPY_MOVABLE(AnnotationController)

public:
	AnnotationController(
		const std::shared_ptr<const ILogicFactory>&  logicFactory,
		std::shared_ptr<const ISettings>             settings,
		std::shared_ptr<const ICollectionProvider>   collectionProvider,
		std::shared_ptr<const IJokeRequesterFactory> jokeRequesterFactory,
		std::shared_ptr<const IDatabaseUser>         databaseUser,
		std::shared_ptr<IFilterProvider>             filterProvider
	);
	~AnnotationController() override;

private: // IAnnotationController
	void    SetCurrentBookId(QString bookId, bool extractNow) override;
	QString CreateAnnotation(const IDataProvider& dataProvider, const IStrategy& strategy) const override;
	void    ShowJokes(IJokeRequesterFactory::Implementation impl, bool value) override;
	void    ShowReviews(bool value) override;

	void RegisterObserver(IObserver* observer) override;
	void UnregisterObserver(IObserver* observer) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
