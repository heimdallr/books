#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IBookSearchController.h"
#include "interface/logic/ICollectionController.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/INavigationQueryExecutor.h"
#include "interface/ui/IUiFactory.h"

class QString;

namespace HomeCompa::Flibrary
{

class SearchController final : virtual public IBookSearchController
{
	NON_COPY_MOVABLE(SearchController)

public:
	SearchController(
		const std::shared_ptr<const ICollectionController>& collectionController,
		std::shared_ptr<IDatabaseUser>                      databaseUser,
		std::shared_ptr<INavigationQueryExecutor>           navigationQueryExecutor,
		std::shared_ptr<IUiFactory>                         uiFactory
	);
	~SearchController() override;

private: // IBookSearchController
	void CreateNew(Callback callback) override;
	void Remove(Ids ids, Callback callback) const override;
	void Search(QString searchString, Callback callback) override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
