#pragma once

#include <functional>
#include <unordered_set>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

class QString;

namespace HomeCompa::Flibrary {

class GroupController
{
	NON_COPY_MOVABLE(GroupController)

public:
	using Callback = std::function<void()>;
	using Id = long long;
	using Ids = std::unordered_set<Id>;

public:
	GroupController(std::shared_ptr<class IDatabaseUser> databaseUser
		, std::shared_ptr<class INavigationQueryExecutor> navigationQueryExecutor
		, std::shared_ptr<class IUiFactory> uiFactory
	);
	~GroupController();

public:
	void CreateNew(Callback callback) const;
	void Remove(Ids ids, Callback callback) const;
	void AddToGroup(Id id, Ids ids, Callback callback) const;
	void RemoveFromGroup(Id id, Ids ids, Callback callback) const;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
