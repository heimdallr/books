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

protected:
	explicit DatabaseUser(std::shared_ptr<ILogicFactory> logicFactory);
	~DatabaseUser();

protected:
	static std::unique_ptr<QTimer> CreateTimer(std::function<void()> f);

private:
	std::unique_ptr<Util::IExecutor> CreateExecutor();

protected:
	PropagateConstPtr<ILogicFactory, std::shared_ptr> m_logicFactory;
	std::unique_ptr<DB::IDatabase> m_db;
	std::unique_ptr<Util::IExecutor> m_executor;
};

}
