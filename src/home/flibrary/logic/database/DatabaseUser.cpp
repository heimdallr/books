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

class IApplicationCursorController  // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IApplicationCursorController() = default;
	virtual void Set(bool value) = 0;
};

class ApplicationCursorControllerStub : public IApplicationCursorController
{
public:
	static std::unique_ptr<IApplicationCursorController> create()
	{
		return std::make_unique<ApplicationCursorControllerStub>();
	}
	void Set(bool) override { }
};

class ApplicationCursorController : public IApplicationCursorController
{
public:
	static std::unique_ptr<IApplicationCursorController> create()
	{
		return std::make_unique<ApplicationCursorController>();
	}
public:
	void Set(const bool value) override
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

std::unique_ptr<IApplicationCursorController> APPLICATION_CURSOR_CONTROLLER { ApplicationCursorControllerStub::create() };

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

	std::unique_ptr<Util::IExecutor> CreateExecutor(const ILogicFactory & logicFactory) const
	{
		return logicFactory.GetExecutor({1
			, [] { }
			, [this] { APPLICATION_CURSOR_CONTROLLER->Set(true); }
			, [this] { APPLICATION_CURSOR_CONTROLLER->Set(false); }
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

void DatabaseUser::EnableApplicationCursorChange(const bool value)
{
	APPLICATION_CURSOR_CONTROLLER = value ? ApplicationCursorController::create() : ApplicationCursorControllerStub::create();
}
