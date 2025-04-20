#include "DatabaseUser.h"

#include <QtGui/QCursor>
#include <QtGui/QGuiApplication>

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/Localization.h"

#include "data/DataItem.h"
#include "util/executor/factory.h"

#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

class IApplicationCursorController // NOLINT(cppcoreguidelines-special-member-functions)
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

	void Set(bool) override
	{
	}
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

} // namespace

struct DatabaseUser::Impl
{
	PropagateConstPtr<IDatabaseController, std::shared_ptr> databaseController;
	std::shared_ptr<Util::IExecutor> executor;

	Impl(const ILogicFactory& logicFactory, std::shared_ptr<IDatabaseController> databaseController)
		: databaseController(std::move(databaseController))
		, executor(CreateExecutor(logicFactory))
	{
	}

	std::unique_ptr<Util::IExecutor> CreateExecutor(const ILogicFactory& logicFactory) const
	{
		return logicFactory.GetExecutor({ 1, [] {}, [this] { APPLICATION_CURSOR_CONTROLLER->Set(true); }, [this] { APPLICATION_CURSOR_CONTROLLER->Set(false); }, [] {} });
	}
};

DatabaseUser::DatabaseUser(const std::shared_ptr<ILogicFactory>& logicFactory, std::shared_ptr<IDatabaseController> databaseController)
	: m_impl(*logicFactory, std::move(databaseController))
{
	PLOGV << "DatabaseUser created";
}

DatabaseUser::~DatabaseUser()
{
	PLOGV << "DatabaseUser destroyed";
}

size_t DatabaseUser::Execute(Util::IExecutor::Task&& task, const int priority) const
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

QVariant DatabaseUser::GetSetting(const Key key, QVariant defaultValue) const
{
	try
	{
		const auto db = m_impl->databaseController->GetDatabase();
		const auto query = db->CreateQuery("select SettingValue from Settings where SettingID = ?");
		query->Bind(0, static_cast<long long>(key));
		query->Execute();
		return query->Eof() ? defaultValue : QString::fromStdString(query->Get<std::string>(0));
	}
	catch (...)
	{
		return defaultValue;
	}
}

void DatabaseUser::SetSetting(const Key key, const QVariant& value) const
{
	const auto db = m_impl->databaseController->GetDatabase();
	const auto tr = db->CreateTransaction();
	const auto command = tr->CreateCommand("insert or replace into Settings(SettingID, SettingValue) values(?, ?)");
	command->Bind(0, static_cast<long long>(key));
	command->Bind(1, value.toString().toStdString());
	command->Execute();
	tr->Commit();
}
