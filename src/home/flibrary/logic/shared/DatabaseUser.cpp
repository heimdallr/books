#include "DatabaseUser.h"

#include <QtGui/QCursor>
#include <QtGui/QGuiApplication>

#include "database/DatabaseController.h"
#include "util/executor/factory.h"

using namespace HomeCompa;
using namespace Flibrary;

struct DatabaseUser::Impl
{
	PropagateConstPtr<DatabaseController, std::shared_ptr> databaseController;
	std::unique_ptr<Util::IExecutor> m_executor;

	Impl(const ILogicFactory & logicFactory
		, std::shared_ptr<DatabaseController> databaseController
	)
		: databaseController(std::move(databaseController))
		, m_executor(CreateExecutor(logicFactory))
	{
	}

	static std::unique_ptr<Util::IExecutor> CreateExecutor(const ILogicFactory & logicFactory)
	{
		return logicFactory.GetExecutor({
			  [] { }
			, [] { QGuiApplication::setOverrideCursor(Qt::BusyCursor); }
			, [] { QGuiApplication::restoreOverrideCursor(); }
			, [] { }
		});
	}
};

DatabaseUser::DatabaseUser(const std::shared_ptr<ILogicFactory> & logicFactory
	, std::shared_ptr<DatabaseController> databaseController
)
	: m_impl(*logicFactory, std::move(databaseController))
{
}

DatabaseUser::~DatabaseUser() = default;

std::unique_ptr<QTimer> DatabaseUser::CreateTimer(std::function<void()> f)
{
	auto timer = std::make_unique<QTimer>();
	timer->setSingleShot(true);
	timer->setInterval(std::chrono::milliseconds(200));
	QObject::connect(timer.get(), &QTimer::timeout, timer.get(), [f = std::move(f)]
	{
		f();
	});

	return timer;
};

size_t DatabaseUser::Execute(Util::IExecutor::Task && task, const int priority) const
{
	return (*m_impl->m_executor)(std::move(task), priority);
}

std::shared_ptr<DB::IDatabase> DatabaseUser::Database() const
{
	return m_impl->databaseController->GetDatabase();
}
