#include "DatabaseUser.h"

#include <QtGui/QCursor>
#include <QtGui/QGuiApplication>

#include "util/executor/factory.h"

using namespace HomeCompa;
using namespace Flibrary;

DatabaseUser::DatabaseUser(std::shared_ptr<ILogicFactory> logicFactory)
	: m_logicFactory(std::move(logicFactory))
	, m_db(std::unique_ptr<DB::IDatabase>{})
	, m_executor(CreateExecutor())
{
}

DatabaseUser::~DatabaseUser() = default;

std::unique_ptr<Util::IExecutor> DatabaseUser::CreateExecutor()
{
	return m_logicFactory->GetExecutor({
		  [&] { m_db = m_logicFactory->GetDatabase(); }
		, [ ] { QGuiApplication::setOverrideCursor(Qt::BusyCursor); }
		, [ ] { QGuiApplication::restoreOverrideCursor(); }
		, [&] { m_db.reset(); }
	});
}

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
