#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IBookSearchController.h"

class QString;

namespace HomeCompa
{
class ISettings;
}

namespace HomeCompa::Flibrary
{

class SearchController final : virtual public IBookSearchController
{
	NON_COPY_MOVABLE(SearchController)

public:
	SearchController(std::shared_ptr<ISettings> settings,
	                 std::shared_ptr<class IDatabaseUser> databaseUser,
	                 std::shared_ptr<class INavigationQueryExecutor> navigationQueryExecutor,
	                 std::shared_ptr<class IUiFactory> uiFactory,
	                 const std::shared_ptr<class ICollectionController>& collectionController);
	~SearchController() override;

private: // IBookSearchController
	void CreateNew(Callback callback) override;
	void Remove(Ids ids, Callback callback) const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
