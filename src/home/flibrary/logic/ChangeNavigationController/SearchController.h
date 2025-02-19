#pragma once

#include <functional>
#include <unordered_set>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

class QString;

namespace HomeCompa
{
class ISettings;
}

namespace HomeCompa::Flibrary
{

class SearchController
{
	NON_COPY_MOVABLE(SearchController)

public:
	using Callback = std::function<void()>;
	using Id = long long;
	using Ids = std::unordered_set<Id>;

public:
	struct SearchMode
	{
#define SEARCH_MODE_ITEMS_X_MACRO \
	SEARCH_MODE_ITEM(Contains)    \
	SEARCH_MODE_ITEM(StartsWith)  \
	SEARCH_MODE_ITEM(EndsWith)    \
	SEARCH_MODE_ITEM(Equals)

		enum
		{

#define SEARCH_MODE_ITEM(NAME) NAME,
			SEARCH_MODE_ITEMS_X_MACRO
#undef SEARCH_MODE_ITEM
		};
	};

public:
	SearchController(std::shared_ptr<ISettings> settings,
	                 std::shared_ptr<class IDatabaseUser> databaseUser,
	                 std::shared_ptr<class INavigationQueryExecutor> navigationQueryExecutor,
	                 std::shared_ptr<class IUiFactory> uiFactory,
	                 const std::shared_ptr<class ICollectionController>& collectionController);
	~SearchController();

public:
	void CreateNew(Callback callback);
	void Remove(Ids ids, Callback callback) const;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
