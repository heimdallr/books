#pragma once

#include <functional>
#include <unordered_set>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/INavigationQueryExecutor.h"
#include "interface/ui/IUiFactory.h"

class QString;

namespace HomeCompa::Flibrary
{

class GroupController
{
	NON_COPY_MOVABLE(GroupController)

public:
	using Callback = std::function<void(long long)>;
	using Id       = long long;
	using Ids      = std::unordered_set<Id>;

public:
	GroupController(std::shared_ptr<IDatabaseUser> databaseUser, std::shared_ptr<INavigationQueryExecutor> navigationQueryExecutor, std::shared_ptr<IUiFactory> uiFactory);
	~GroupController();

public:
	void CreateNew(Callback callback) const;
	void Rename(Id id, QString name, Callback callback) const;
	void Remove(Ids ids, Callback callback) const;
	void AddToGroup(Id id, Ids ids, Callback callback) const;
	void RemoveFromGroup(Id id, Ids ids, Callback callback) const;
	void RemoveBooks(Id id, Ids ids, Callback callback) const;
	void RemoveAuthors(Id id, Ids ids, Callback callback) const;
	void RemoveSeries(Id id, Ids ids, Callback callback) const;
	void RemoveKeywords(Id id, Ids ids, Callback callback) const;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
