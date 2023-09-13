#pragma once

#include <functional>
#include <unordered_set>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

class QString;

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Flibrary {

class SearchController
{
	NON_COPY_MOVABLE(SearchController)

public:
	using Callback = std::function<void()>;
	using Id = long long;
	using Ids = std::unordered_set<Id>;

public:
	SearchController(std::shared_ptr<ISettings> settings
		, std::shared_ptr<class DatabaseUser> databaseUser
		, std::shared_ptr<class IUiFactory> uiFactory
	);
	~SearchController();

public:
	void CreateNew(Callback callback);
	void Remove(Ids ids, Callback callback) const;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
