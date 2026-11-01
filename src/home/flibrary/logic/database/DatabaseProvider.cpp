#include "DatabaseProvider.h"

#include <mutex>
#include <thread>

#include <QtGui/QCursor>
#include <QtGui/QGuiApplication>

#include "fnd/StrUtil.h"
#include "fnd/observable.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "database/interface/ITransaction.h"

#include "data/DataItem.h"
#include "database/factory/Factory.h"
#include "util/IExecutor.h"
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

class ThreadHandled final : public Util::IExecutor::IThreadHandled
{
public:
	explicit ThreadHandled(std::shared_ptr<DB::IDatabase> db)
		: m_db { std::move(db) }
	{
	}

private:
	std::shared_ptr<DB::IDatabase> m_db;
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

void Pragma(DB::IDatabase& db)
{
	db.CreateQuery("PRAGMA synchronous = NORMAL;")->Execute();
	db.CreateQuery("PRAGMA foreign_keys = ON;")->Execute();
	db.CreateQuery("PRAGMA cache_size = -10000;")->Execute();
	db.CreateQuery("PRAGMA busy_timeout = 5000;")->Execute();
}

template <typename T>
std::unique_ptr<T> CreateImpl(const ICollectionProvider& collectionProvider)
{
	if (!collectionProvider.ActiveCollectionExists())
		return {};

	const auto databaseName = collectionProvider.GetActiveCollection().GetDatabase();
	if (databaseName.isEmpty())
		return {};
	try
	{
		auto db = Create<T>(DB::Factory::Impl::Sqlite, std::format("path={}", databaseName));
		Pragma(*db);
		return db;
	}
	catch (const std::exception& ex)
	{
		PLOGW << ex.what();
	}
	return {};
}

auto GetThreadId()
{
	return std::hash<std::thread::id> {}(std::this_thread::get_id());
}

} // namespace

class DatabaseProvider::Impl final
    : public Observable<IObserver>
    , DB::IDatabaseObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(const ILogicFactory& logicFactory, std::shared_ptr<const ICollectionProvider> collectionProvider)
		: m_collectionProvider { std::move(collectionProvider) }
		, m_mainThreadDb { CreateImpl<DB::IDatabaseWritable>(*m_collectionProvider) }
		, m_executor { CreateExecutor(logicFactory) }
	{
		if (!m_mainThreadDb)
			return;

		[[maybe_unused]] const auto ok = m_dbList.try_emplace(GetThreadId(), m_mainThreadDb).second;
		assert(ok);
		m_mainThreadDb->RegisterObserver(this);
	}

	~Impl() override
	{
		m_executor.reset();
		m_mainThreadDb.reset();
		if (const auto db = GetDatabaseWritable())
			db->CreateQuery("PRAGMA wal_checkpoint(TRUNCATE);")->Execute();
	}

	std::shared_ptr<DB::IDatabase> GetDatabase()
	{
		return GetEnlistedDatabase().second;
	}

	std::unique_ptr<DB::IDatabaseWritable> GetDatabaseWritable() const
	{
		return CreateImpl<DB::IDatabaseWritable>(*m_collectionProvider);
	}

	size_t Execute(Task<DB::IDatabase>&& task, const int priority)
	{
		return (*m_executor)(
			{ std::move(task.name),
			  [this, t = std::move(task.task)](Util::IExecutor::ThreadContext& ctx) {
				  auto [threadId, db] = GetEnlistedDatabase();
				  auto& dbRef         = *db;
				  if (threadId)
				  {
					  {
						  std::lock_guard lock(m_dbListGuard);
						  m_dbList.try_emplace(threadId, db);
					  }
					  ctx.Add(std::make_unique<ThreadHandled>(std::move(db)));
				  }
				  return t(dbRef);
			  } },
			priority
		);
	}

	size_t Execute(Task<DB::IDatabaseWritable>&& task, const int priority) const
	{
		return (*m_executor)(
			{ std::move(task.name),
			  [this, t = std::move(task.task)](Util::IExecutor::ThreadContext&) {
				  const auto db = GetDatabaseWritable();
				  assert(db);
				  return t(*db);
			  } },
			priority
		);
	}

	bool IsDatabaseValid() const noexcept
	{
		return !!m_mainThreadDb;
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
	std::pair<size_t, std::shared_ptr<DB::IDatabase>> GetEnlistedDatabase()
	{
		const auto threadId = GetThreadId();
		{
			std::lock_guard lock(m_dbListGuard);
			if (const auto it = m_dbList.find(threadId); it != m_dbList.end())
				return std::make_pair(0, it->second.lock());
		}

		std::shared_ptr db = CreateImpl<DB::IDatabase>(*m_collectionProvider);
		assert(db);
		return std::make_pair(threadId, std::move(db));
	}

private:
	std::shared_ptr<const ICollectionProvider>               m_collectionProvider;
	std::mutex                                               m_dbListGuard;
	std::unordered_map<size_t, std::weak_ptr<DB::IDatabase>> m_dbList;
	std::shared_ptr<DB::IDatabaseWritable>                   m_mainThreadDb;
	std::shared_ptr<Util::IExecutor>                         m_executor;
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

size_t DatabaseProvider::Execute(Task<DB::IDatabase>&& task, const int priority) const
{
	return m_impl->Execute(std::move(task), priority);
}

size_t DatabaseProvider::ExecuteWritable(Task<DB::IDatabaseWritable>&& task, const int priority) const
{
	return m_impl->Execute(std::move(task), priority);
}

bool DatabaseProvider::IsDatabaseValid() const noexcept
{
	return m_impl->IsDatabaseValid();
}

void DatabaseProvider::Reset()
{
	m_impl.reset();
}

std::shared_ptr<DB::IDatabase> DatabaseProvider::GetDatabase() const
{
	return m_impl->GetDatabase();
}

std::unique_ptr<DB::IDatabaseWritable> DatabaseProvider::GetDatabaseWritable() const
{
	return m_impl->GetDatabaseWritable();
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
		const auto db    = m_impl->GetDatabase();
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
	const auto db      = m_impl->GetDatabaseWritable();
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
