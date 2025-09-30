#include "Database.h"

#include <mutex>
#include <numeric>
#include <sstream>
#include <string>

#include "fnd/FindPair.h"
#include "fnd/NonCopyMovable.h"
#include "fnd/observable.h"

#include "IDatabase.h"
#include "IQuery.h"
#include "ITemporaryTable.h"
#include "ITransaction.h"
#include "log.h"
#include "sqlite3ppext.h"

namespace HomeCompa::DB::Impl::Sqlite
{

std::unique_ptr<ITransaction>    CreateTransactionImpl(std::mutex& mutex, sqlite3pp::database& db);
std::unique_ptr<IQuery>          CreateQueryImpl(std::mutex& mutex, sqlite3pp::database& db, std::string_view query);
std::unique_ptr<ITemporaryTable> CreateTemporaryTableImpl(IDatabase& db, const std::vector<std::string_view>& fields, const std::vector<std::string_view>& additional);

namespace
{

constexpr auto PATH      = "path";
constexpr auto EXTENSION = "extension";
constexpr auto FLAG      = "flag";

using ConnectionParameters = std::multimap<std::string, std::string, std::less<>>;

using ObserverMethod = void (IDatabaseObserver::*)(std::string_view dbName, std::string_view tableName, int64_t rowId);
// ocCodes: sqlite3.cpp
constexpr std::pair<int, ObserverMethod> g_opCodeToObserverMethod[] {
	{ 18, &IDatabaseObserver::OnInsert },
	{  9, &IDatabaseObserver::OnDelete },
	{ 23, &IDatabaseObserver::OnUpdate },
};

constexpr std::pair<const char*, int> g_openFlags[] {
#define ITEM(NAME) { #NAME, SQLITE_OPEN_##NAME }
	ITEM(READONLY),     ITEM(READWRITE),    ITEM(CREATE),       ITEM(DELETEONCLOSE), ITEM(EXCLUSIVE),      ITEM(AUTOPROXY), ITEM(URI),       ITEM(MEMORY),      ITEM(MAIN_DB),      ITEM(TEMP_DB),
	ITEM(TRANSIENT_DB), ITEM(MAIN_JOURNAL), ITEM(TEMP_JOURNAL), ITEM(SUBJOURNAL),    ITEM(MASTER_JOURNAL), ITEM(NOMUTEX),   ITEM(FULLMUTEX), ITEM(SHAREDCACHE), ITEM(PRIVATECACHE), ITEM(WAL),
#undef ITEM
};

std::vector<std::string> Split(const std::string& src, const char separator)
{
	std::vector<std::string> result;
	std::istringstream       stream(src);
	std::string              s;
	while (std::getline(stream, s, separator))
		result.push_back(s);

	return result;
}

ConnectionParameters ParseConnectionString(const std::string& connection)
{
	ConnectionParameters result;
	for (const auto& parameter : Split(connection, ';'))
	{
		auto pair = Split(parameter, '=');
		if (std::size(pair) == 1)
			result.emplace(PATH, std::move(pair.front()));
		else
			result.emplace(std::move(pair.front()), std::move(pair.back()));
	}

	return result;
}

int GetOpenFlags(const ConnectionParameters& parameters)
{
	auto [begin, end] = parameters.equal_range(FLAG);
	auto result       = std::accumulate(begin, end, 0, [](const int init, const auto& item) {
        return init | FindSecond(g_openFlags, item.second.data(), 0, PszComparer {});
    });
	if (result & SQLITE_OPEN_CREATE)
		result |= SQLITE_OPEN_READWRITE;

	return result ? result : SQLITE_OPEN_READWRITE;
}

const std::string& GetValue(const ConnectionParameters& parameters, const std::string& key)
{
	auto [begin, end] = parameters.equal_range(key);
	if (begin == end)
		throw std::invalid_argument("must pass path");

	return begin->second;
}

class DatabaseFunctionContext : virtual public DB::DatabaseFunctionContext
{
public:
	explicit DatabaseFunctionContext(sqlite3pp::ext::context& ctx)
		: m_ctx(ctx)
	{
	}

	void SetResult(const int value) override
	{
		m_ctx.result(value);
	}

private:
	sqlite3pp::ext::context& m_ctx;
};

class Database final
	: virtual public IDatabase
	, public Observable<IDatabaseObserver>
{
	NON_COPY_MOVABLE(Database)

private:
	struct ObserverImpl
	{
		explicit ObserverImpl(Database& self)
			: m_self(self)
		{
		}

		void OnUpdate(const int opCode, std::string_view dbName, std::string_view tableName, const int64_t rowId) const
		{
			const auto method = FindSecond(g_opCodeToObserverMethod, opCode);
			m_self.Perform(method, dbName, tableName, rowId);
		}

	private:
		Database& m_self;
	};

public:
	explicit Database(const std::string& connection)
		: m_connectionParameters(ParseConnectionString(connection))
		, m_db(GetValue(m_connectionParameters, PATH).data(), GetOpenFlags(m_connectionParameters))
		, m_observer(*this)
	{
		for (auto [begin, end] = m_connectionParameters.equal_range(EXTENSION); begin != end; ++begin)
			m_db.load_extension(begin->second.data());

		m_db.set_update_handler([&](const int opCode, const char* dbName, const char* tableName, const int64_t rowId) {
			m_observer.OnUpdate(opCode, dbName, tableName, rowId);
		});

		[[maybe_unused]] const auto ok = Database::CreateQuery("ATTACH DATABASE ':memory:' AS tmp;")->Execute();
		assert(ok);

		PLOGV << "database created";
	}

	~Database() override
	{
		PLOGV << "database destroyed";
	}

private: // Database
	[[nodiscard]] std::unique_ptr<ITransaction> CreateTransaction() override
	{
		return CreateTransactionImpl(m_guard, m_db);
	}

	[[nodiscard]] std::unique_ptr<IQuery> CreateQuery(const std::string_view query) override
	{
		return CreateQueryImpl(m_guard, m_db, query);
	}

	[[nodiscard]] std::unique_ptr<ITemporaryTable> CreateTemporaryTable(const std::vector<std::string_view>& fields, const std::vector<std::string_view>& additional) override
	{
		return CreateTemporaryTableImpl(*this, fields, additional);
	}

	void CreateFunction(const std::string_view name, DatabaseFunction function) override
	{
		m_functions.try_emplace(std::string(name), std::make_unique<sqlite3pp::ext::function>(m_db)).first->second->create(name.data(), [function = std::move(function)](sqlite3pp::ext::context& ctx) {
			DatabaseFunctionContext context(ctx);
			function(context);
		});
	}

	void RegisterObserver(IDatabaseObserver* observer) override
	{
		Register(observer);
	}

	void UnregisterObserver(IDatabaseObserver* observer) override
	{
		Unregister(observer);
	}

private:
	ConnectionParameters                                                          m_connectionParameters;
	std::mutex                                                                    m_guard;
	sqlite3pp::database                                                           m_db;
	ObserverImpl                                                                  m_observer;
	std::map<std::string, std::unique_ptr<sqlite3pp::ext::function>, std::less<>> m_functions;
};

} // namespace

std::unique_ptr<IDatabase> CreateDatabase(const std::string& connection)
{
	return std::make_unique<Database>(connection);
}

} // namespace HomeCompa::DB::Impl::Sqlite
