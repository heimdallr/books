#pragma once

#include <QTimer>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IDatabaseController.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/ILogicFactory.h"

namespace HomeCompa::Flibrary
{

class DatabaseUser : virtual public IDatabaseUser
{
	NON_COPY_MOVABLE(DatabaseUser)

public:
	DatabaseUser(const std::shared_ptr<ILogicFactory>& logicFactory, std::shared_ptr<IDatabaseController> databaseController);
	~DatabaseUser() override;

public:
	size_t Execute(Util::IExecutor::Task&& task, int priority = 0) const override;
	std::shared_ptr<DB::IDatabase> Database() const override;
	std::shared_ptr<DB::IDatabase> CheckDatabase() const override;
	std::shared_ptr<Util::IExecutor> Executor() const override;
	void EnableApplicationCursorChange(bool value) override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
