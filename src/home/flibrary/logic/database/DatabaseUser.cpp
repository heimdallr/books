#include "DatabaseUser.h"

#include <QtGui/QCursor>
#include <QtGui/QGuiApplication>

#include "fnd/StrUtil.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "data/DataItem.h"
#include "database/factory/Factory.h"
#include "util/executor/factory.h"

#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

class IApplicationCursorController // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IApplicationCursorController() = default;
	virtual void Set(bool value)            = 0;
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

auto APPLICATION_CURSOR_CONTROLLER { ApplicationCursorControllerStub::create() };

std::unique_ptr<Util::IExecutor> CreateExecutor(const ILogicFactory& logicFactory)
{
	return logicFactory.GetExecutor(
		{ 1,
		  [] {
		  },
		  [] {
			  APPLICATION_CURSOR_CONTROLLER->Set(true);
		  },
		  [] {
			  APPLICATION_CURSOR_CONTROLLER->Set(false);
		  },
		  [] {
		  } }
	);
}

} // namespace

struct DatabaseUser::Impl
{
	std::shared_ptr<Util::IExecutor> executor;

	Impl(const ILogicFactory& logicFactory, std::shared_ptr<const ICollectionProvider> collectionProvider)
		: executor { CreateExecutor(logicFactory) }
		, m_collectionProvider { std::move(collectionProvider) }
	{
		std::ignore = GetDatabase();
	}

	std::shared_ptr<DB::IDatabase> GetDatabase(const bool checkOnly = false)
	{
		const auto threadId = std::hash<std::thread::id> {}(std::this_thread::get_id());
		{
			std::lock_guard lock(m_dbListGuard);
			if (const auto it = m_dbList.find(threadId); it != m_dbList.end())
				return it->second;
		}

		if (checkOnly || !m_collectionProvider->ActiveCollectionExists())
			return {};

		const auto databaseName = m_collectionProvider->GetActiveCollection().GetDatabase();
		if (databaseName.isEmpty())
			return {};

		std::shared_ptr db = Create(DB::Factory::Impl::Sqlite, std::format("path={}", databaseName));
		db->CreateQuery("PRAGMA synchronous = NORMAL;")->Execute();
		db->CreateQuery("PRAGMA foreign_keys = ON;")->Execute();
		db->CreateQuery("PRAGMA cache_size = -10000;")->Execute();
		db->CreateQuery("PRAGMA busy_timeout = 5000;")->Execute();

		std::lock_guard lock(m_dbListGuard);
		return m_dbList.try_emplace(threadId, std::move(db)).first->second;
	}

	void Reset()
	{
		m_dbList.clear();
	}

private:
	std::shared_ptr<const ICollectionProvider>                 m_collectionProvider;
	std::mutex                                                 m_dbListGuard;
	std::unordered_map<size_t, std::shared_ptr<DB::IDatabase>> m_dbList;
};

DatabaseUser::DatabaseUser(const std::shared_ptr<ILogicFactory>& logicFactory, std::shared_ptr<const ICollectionProvider> collectionProvider)
	: m_impl(std::make_unique<Impl>(*logicFactory, std::move(collectionProvider)))
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

void DatabaseUser::Reset()
{
	m_impl->Reset();
}

std::shared_ptr<DB::IDatabase> DatabaseUser::GetDatabase() const
{
	return m_impl->GetDatabase();
}

std::shared_ptr<DB::IDatabase> DatabaseUser::CheckDatabase() const
{
	return m_impl->GetDatabase(true);
}

std::shared_ptr<Util::IExecutor> DatabaseUser::Executor() const
{
	return m_impl->executor;
}

void DatabaseUser::EnableApplicationCursorChange(const bool value)
{
	APPLICATION_CURSOR_CONTROLLER = value ? ApplicationCursorController::create() : ApplicationCursorControllerStub::create();
}

QVariant DatabaseUser::GetSetting(const Key key, QVariant defaultValueSrc) const
{
	auto defaultValue = std::move(defaultValueSrc);

	try
	{
		const auto db = m_impl->GetDatabase();
		if (!db)
			return {};

		const auto query = db->CreateQuery("select SettingValue from Settings where SettingID = ?", static_cast<long long>(key));
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
	const auto db      = m_impl->GetDatabase();
	const auto tr      = db->CreateTransaction();
	const auto command = tr->CreateCommand("insert or replace into Settings(SettingID, SettingValue) values(?, ?)");
	command->Execute(static_cast<long long>(key), value.toString());
	tr->Commit();
}
