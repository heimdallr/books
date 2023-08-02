#pragma once

#include <QTimer>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/ILogicFactory.h"

#include "util/IExecutor.h"
#include "database/interface/IDatabase.h"

namespace HomeCompa::Flibrary {

class DatabaseUser
{
	NON_COPY_MOVABLE(DatabaseUser)

public:
	DatabaseUser(const std::shared_ptr<ILogicFactory> & logicFactory
		, std::shared_ptr<class DatabaseController> databaseController
	);
	~DatabaseUser();

public:
	size_t Execute(Util::IExecutor::Task && task, int priority = 0) const;
	std::shared_ptr<DB::IDatabase> Database() const;

public:
	static std::unique_ptr<QTimer> CreateTimer(std::function<void()> f);

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
