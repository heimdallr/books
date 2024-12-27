#include "DatabaseUser.h"

#include <QtGui/QCursor>
#include <QtGui/QGuiApplication>

#include <plog/Log.h>

#include "data/DataItem.h"
#include "interface/constants/Localization.h"
#include "interface/logic/IDatabaseController.h"
#include "util/executor/factory.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

class ApplicationCursorWrapper
{
public:
	void Set(const bool value)
	{
		if (!m_counter)
			QGuiApplication::setOverrideCursor(Qt::BusyCursor);

		m_counter += value ? 1 : -1;

		if (!m_counter)
			QGuiApplication::restoreOverrideCursor();
	}

private:
	int m_counter { 0 };
};

ApplicationCursorWrapper APPLICATION_CURSOR_WRAPPER;

}

struct DatabaseUser::Impl
{
	PropagateConstPtr<IDatabaseController, std::shared_ptr> databaseController;
	std::shared_ptr<Util::IExecutor> executor;

	Impl(const ILogicFactory & logicFactory
		, std::shared_ptr<IDatabaseController> databaseController
	)
		: databaseController(std::move(databaseController))
		, executor(CreateExecutor(logicFactory))
	{
	}

	static std::unique_ptr<Util::IExecutor> CreateExecutor(const ILogicFactory & logicFactory)
	{
		return logicFactory.GetExecutor({1
			, [] { }
			, [] { APPLICATION_CURSOR_WRAPPER.Set(true); }
			, [] { APPLICATION_CURSOR_WRAPPER.Set(false); }
			, [] { }
		});
	}
};

DatabaseUser::DatabaseUser(const std::shared_ptr<ILogicFactory> & logicFactory
	, std::shared_ptr<IDatabaseController> databaseController
)
	: m_impl(*logicFactory, std::move(databaseController))
{
	PLOGD << "DatabaseUser created";
}

DatabaseUser::~DatabaseUser()
{
	PLOGD << "DatabaseUser destroyed";
}

size_t DatabaseUser::Execute(Util::IExecutor::Task && task, const int priority) const
{
	return (*m_impl->executor)(std::move(task), priority);
}

std::shared_ptr<DB::IDatabase> DatabaseUser::Database() const
{
	return m_impl->databaseController->GetDatabase();
}

std::shared_ptr<DB::IDatabase> DatabaseUser::CheckDatabase() const
{
	return m_impl->databaseController->CheckDatabase();
}

std::shared_ptr<Util::IExecutor> DatabaseUser::Executor() const
{
	return m_impl->executor;
}
