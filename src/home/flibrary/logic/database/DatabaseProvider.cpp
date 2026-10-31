#include "DatabaseProvider.h"

#include <QtGui/QCursor>
#include <QtGui/QGuiApplication>

#include "fnd/StrUtil.h"
#include "fnd/observable.h"

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

struct DatabaseProvider::Impl final
    : Observable<IObserver>
    , DB::IDatabaseObserver
{
	std::shared_ptr<Util::IExecutor> executor;

	Impl(const ILogicFactory& logicFactory, std::shared_ptr<const ICollectionProvider> collectionProvider)
		: executor { CreateExecutor(logicFactory) }
		, m_collectionProvider { std::move(collectionProvider) }
	{
		if (const auto db = GetDatabase())
			db->RegisterObserver(this);
	}

	std::shared_ptr<DB::IDatabase> GetDatabase()
	{
		const auto threadId = std::hash<std::thread::id> {}(std::this_thread::get_id());
		{
			std::lock_guard lock(m_dbListGuard);
			if (const auto it = m_dbList.find(threadId); it != m_dbList.end())
				return it->second;
		}

		const auto databaseName = m_collectionProvider->GetActiveCollection().GetDatabase();
		if (databaseName.isEmpty())
			return {};

		std::shared_ptr db = CreateWritable(DB::Factory::Impl::Sqlite, std::format("path={}", databaseName));
		Pragma(*db);

		std::lock_guard lock(m_dbListGuard);
		return m_dbList.try_emplace(threadId, std::move(db)).first->second;
	}

	std::shared_ptr<DB::IDatabaseWritable> GetDatabaseWritable()
	{
		const auto databaseName = m_collectionProvider->GetActiveCollection().GetDatabase();
		if (databaseName.isEmpty())
			return {};

		std::shared_ptr db = CreateWritable(DB::Factory::Impl::Sqlite, std::format("path={}", databaseName));
		Pragma(*db);
		return db;
	}

	void Reset()
	{
		m_dbList.clear();
	}

private: // DB::IDatabaseObserver
	void OnInsert(const std::string_view dbName, const std::string_view tableName, const int64_t rowId) override
	{
		Perform(&IObserver::OnInsert, dbName, tableName, rowId);
	}

	void OnUpdate(const std::string_view dbName, const std::string_view tableName, const int64_t rowId) override
	{
		Perform(&IObserver::OnUpdate, dbName, tableName, rowId);
	}

	void OnDelete(const std::string_view dbName, const std::string_view tableName, const int64_t rowId) override
	{
		Perform(&IObserver::OnDelete, dbName, tableName, rowId);
	}

private:
	void Pragma(DB::IDatabase& db)
	{
		db.CreateQuery("PRAGMA synchronous = NORMAL;")->Execute();
		db.CreateQuery("PRAGMA foreign_keys = ON;")->Execute();
		db.CreateQuery("PRAGMA cache_size = -10000;")->Execute();
		db.CreateQuery("PRAGMA busy_timeout = 5000;")->Execute();
	}

private:
	std::shared_ptr<const ICollectionProvider>                 m_collectionProvider;
	std::mutex                                                 m_dbListGuard;
	std::unordered_map<size_t, std::shared_ptr<DB::IDatabase>> m_dbList;
};

DatabaseProvider::DatabaseProvider(const std::shared_ptr<ILogicFactory>& logicFactory, std::shared_ptr<const ICollectionProvider> collectionProvider)
	: m_impl(std::make_unique<Impl>(*logicFactory, std::move(collectionProvider)))
{
	PLOGV << "DatabaseUser created";
}

DatabaseProvider::~DatabaseProvider()
{
	PLOGV << "DatabaseUser destroyed";
}

size_t DatabaseProvider::Execute(Util::IExecutor::Task&& task, const int priority) const
{
	return (*m_impl->executor)(std::move(task), priority);
}

void DatabaseProvider::Reset()
{
	m_impl->Reset();
}

std::shared_ptr<DB::IDatabase> DatabaseProvider::GetDatabase() const
{
	return m_impl->GetDatabase();
}

std::shared_ptr<DB::IDatabaseWritable> DatabaseProvider::GetDatabaseWritable() const
{
	return m_impl->GetDatabaseWritable();
}

std::shared_ptr<Util::IExecutor> DatabaseProvider::Executor() const
{
	return m_impl->executor;
}

void DatabaseProvider::EnableApplicationCursorChange(const bool value)
{
	APPLICATION_CURSOR_CONTROLLER = value ? ApplicationCursorController::create() : ApplicationCursorControllerStub::create();
}

QVariant DatabaseProvider::GetSetting(const Key key, QVariant defaultValueSrc) const
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

void DatabaseProvider::SetSetting(const Key key, const QVariant& value) const
{
	const auto db      = GetDatabaseWritable();
	const auto tr      = db->CreateTransaction();
	const auto command = tr->CreateCommand("insert or replace into Settings(SettingID, SettingValue) values(?, ?)");
	command->Execute(static_cast<long long>(key), value.toString());
	tr->Commit();
}

void DatabaseProvider::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void DatabaseProvider::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
