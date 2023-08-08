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
	using Ids = std::unordered_set<long long>;

public:
	explicit GroupController(std::shared_ptr<class DatabaseUser> databaseUser);
	~GroupController();

public:
	void CreateNewGroup(QString name, Callback callback) const;
	void RemoveGroups(Ids ids, Callback callback) const;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
