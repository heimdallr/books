#include <latch>
#include <thread>

#include <gtest/gtest.h>

#include "interface/logic/IDatabaseProvider.h"

#include "Hypodermic/Hypodermic.h"
#include "mock/database/MockCommand.h"
#include "mock/database/MockDatabase.h"
#include "mock/database/MockDatabaseFactory.h"
#include "mock/database/MockQuery.h"
#include "mock/database/MockTransaction.h"
#include "mock/logic/MockCollectionProvider.h"
#include "mock/logic/MockLogicFactory.h"
#include "mock/util/MockExecutor.h"

#include "di.h"

namespace HomeCompa::Flibrary::Test {

struct CollectionTest : Collection
{
	CollectionTest()
	{
		id         = "testId";
		name       = "testName";
		m_database = "file::memory:?cache=shared";
	}

	~CollectionTest()
	{
	}
};

}

using namespace HomeCompa::Flibrary::Test;
using namespace HomeCompa::Flibrary;
using namespace HomeCompa::Util;
using namespace HomeCompa::Util::Test;
using namespace HomeCompa;

namespace {

struct DatabaseProviderTest : testing::Test
{
	CollectionTest collection;

	std::function<void(DB::Test::MockDatabase&)>         setupDatabase { [](auto&) {
	} };
	std::function<void(DB::Test::MockDatabaseWritable&)> setupDatabaseWritable { [](auto&) {
	} };
	std::function<void(DB::Test::MockQuery&)>            setupQuery { [](auto&) {
	} };

	std::shared_ptr<MockLogicFactory>       logicFactory { std::make_shared<testing::StrictMock<MockLogicFactory>>() };
	std::shared_ptr<MockCollectionProvider> collectionProvider { std::make_shared<testing::StrictMock<MockCollectionProvider>>() };
	std::shared_ptr<DB::Test::MockFactory>  databaseFactory { std::make_shared<testing::StrictMock<DB::Test::MockFactory>>() };

	DI di { [this](Hypodermic::ContainerBuilder& builder, Hypodermic::Container& /*container*/) {
		builder.registerInstance(databaseFactory).as<DB::IFactory>();
		builder.registerInstance(collectionProvider).as<ICollectionProvider>();
		builder.registerInstance(logicFactory).as<ILogicFactory>();
	} };

	template <typename T>
	void SetupDatabase(T& db) const = delete;

	void SetUp() override;

	~DatabaseProviderTest() override
	{
		setupDatabaseWritable = [](auto&) {
		};
		setupDatabase = [](auto&) {
		};
		setupQuery = [](auto&) {
		};
	}
};

template <>
void DatabaseProviderTest::SetupDatabase<DB::Test::MockDatabase>(DB::Test::MockDatabase& db) const
{
	setupDatabase(db);
}

template <>
void DatabaseProviderTest::SetupDatabase<DB::Test::MockDatabaseWritable>(DB::Test::MockDatabaseWritable& db) const
{
	setupDatabaseWritable(db);
}

void DatabaseProviderTest::SetUp()
{
	const auto createDb = [&]<typename T>() -> std::unique_ptr<T> {
		auto db = std::make_unique<T>();
		SetupDatabase<T>(*db);

		EXPECT_CALL(*db, CreateQuery).Times(testing::AnyNumber()).WillRepeatedly([&](const auto) {
			auto query = std::make_unique<testing::StrictMock<DB::Test::MockQuery>>();
			EXPECT_CALL(*query, Execute).Times(testing::AnyNumber()).WillRepeatedly(testing::Return(true));
			setupQuery(*query);
			return query;
		});

		return db;
	};

	EXPECT_CALL(*databaseFactory, Create(testing::_, testing::_)).Times(testing::AnyNumber()).WillRepeatedly([createDb](const auto, const auto&) {
		return createDb.operator()<DB::Test::MockDatabase>();
	});

	EXPECT_CALL(*databaseFactory, CreateWritable(testing::_, testing::_)).Times(testing::AnyNumber()).WillRepeatedly([createDb](const auto, const auto&) {
		return createDb.operator()<DB::Test::MockDatabaseWritable>();
	});
}

std::unique_ptr<IExecutor> CreateExecutor(const std::function<void(MockExecutor&)> f = [](auto&) {
})
{
	auto executor = std::make_unique<testing::StrictMock<MockExecutor>>();
	f(*executor);
	return executor;
}

TEST_F(DatabaseProviderTest, Stub)
{
	SUCCEED();
}

TEST_F(DatabaseProviderTest, CreateWithoutActiveCollection)
{
	EXPECT_CALL(*logicFactory, GetExecutor).WillOnce(testing::Return(CreateExecutor()));
	EXPECT_CALL(*collectionProvider, ActiveCollectionExists).Times(2).WillRepeatedly(testing::Return(false));
	const auto databaseProvider = di.Resolve<IDatabaseProvider>();
	EXPECT_TRUE(!!databaseProvider);
	EXPECT_FALSE(databaseProvider->IsDatabaseValid());
}

TEST_F(DatabaseProviderTest, CreateValid)
{
	EXPECT_CALL(*logicFactory, GetExecutor).WillOnce(testing::Return(CreateExecutor()));
	EXPECT_CALL(*collectionProvider, ActiveCollectionExists).Times(2).WillRepeatedly(testing::Return(true));
	EXPECT_CALL(testing::Const(*collectionProvider), GetActiveCollection()).Times(2).WillRepeatedly(testing::ReturnRef(collection));
	const auto databaseProvider = di.Resolve<IDatabaseProvider>();
	EXPECT_TRUE(!!databaseProvider);
	EXPECT_TRUE(databaseProvider->IsDatabaseValid());
}

TEST_F(DatabaseProviderTest, GetDatabase)
{
	EXPECT_CALL(*logicFactory, GetExecutor).WillOnce(testing::Return(CreateExecutor()));
	EXPECT_CALL(*collectionProvider, ActiveCollectionExists).WillRepeatedly(testing::Return(true));
	EXPECT_CALL(testing::Const(*collectionProvider), GetActiveCollection()).WillRepeatedly(testing::ReturnRef(collection));
	const auto databaseProvider = di.Resolve<IDatabaseProvider>();

	const auto db1 = databaseProvider->GetDatabase();
	EXPECT_TRUE(!!db1);

	{
		const auto db2 = databaseProvider->GetDatabase();
		EXPECT_TRUE(!!db2);
		EXPECT_EQ(db1.get(), db2.get());
	}

	std::shared_ptr<DB::IDatabase> db3;
	std::jthread([&] {
		db3 = databaseProvider->GetDatabase();
	}).join();
	EXPECT_NE(db1.get(), db3.get());

	std::shared_ptr<DB::IDatabase> db4, db5;
	std::jthread([&] {
		db4 = databaseProvider->GetDatabase();
		db5 = databaseProvider->GetDatabase();
	}).join();
	const std::set dbSet { db1.get(), db3.get(), db4.get(), db5.get() };
	EXPECT_EQ(dbSet.size(), 4);
}

TEST_F(DatabaseProviderTest, GetDatabaseWritable)
{
	EXPECT_CALL(*logicFactory, GetExecutor).WillOnce(testing::Return(CreateExecutor()));
	EXPECT_CALL(*collectionProvider, ActiveCollectionExists).WillRepeatedly(testing::Return(true));
	EXPECT_CALL(testing::Const(*collectionProvider), GetActiveCollection()).WillRepeatedly(testing::ReturnRef(collection));

	setupDatabaseWritable = [](DB::Test::MockDatabaseWritable& db) {
		EXPECT_CALL(db, RegisterObserver(testing::_)).WillOnce([](auto*) {
		});
	};

	const auto databaseProvider = di.Resolve<IDatabaseProvider>();

	const auto db1 = databaseProvider->GetDatabaseWritable();
	EXPECT_TRUE(!!db1);

	{
		const auto db2 = databaseProvider->GetDatabaseWritable();
		EXPECT_TRUE(!!db2);
		EXPECT_NE(db1.get(), db2.get());
	}

	std::shared_ptr<DB::IDatabaseWritable> db3;
	std::jthread([&] {
		db3 = databaseProvider->GetDatabaseWritable();
	}).join();
	EXPECT_NE(db1.get(), db3.get());

	std::shared_ptr<DB::IDatabaseWritable> db4, db5;
	std::jthread([&] {
		db4 = databaseProvider->GetDatabaseWritable();
		db5 = databaseProvider->GetDatabaseWritable();
	}).join();
	const std::set dbSet { db1.get(), db3.get(), db4.get(), db5.get() };
	EXPECT_EQ(dbSet.size(), 4);
}

TEST_F(DatabaseProviderTest, GetSettingsDefault)
{
	EXPECT_CALL(*logicFactory, GetExecutor).WillOnce(testing::Return(CreateExecutor()));
	EXPECT_CALL(*collectionProvider, ActiveCollectionExists).WillRepeatedly(testing::Return(true));
	EXPECT_CALL(testing::Const(*collectionProvider), GetActiveCollection()).WillRepeatedly(testing::ReturnRef(collection));
	const auto databaseProvider = di.Resolve<IDatabaseProvider>();
	setupQuery                  = [](DB::Test::MockQuery& query) {
		EXPECT_CALL(query, BindLong(testing::Matcher<size_t>(testing::_), testing::_)).WillOnce(testing::Return(0));
		EXPECT_CALL(query, Eof).WillOnce(testing::Return(true));
	};
	EXPECT_EQ(databaseProvider->GetSetting(IDatabaseProvider::Key::DatabaseVersion, QString("defaultValue")).toString(), "defaultValue");
}

TEST_F(DatabaseProviderTest, GetSettings)
{
	EXPECT_CALL(*logicFactory, GetExecutor).WillOnce(testing::Return(CreateExecutor()));
	EXPECT_CALL(*collectionProvider, ActiveCollectionExists).WillRepeatedly(testing::Return(true));
	EXPECT_CALL(testing::Const(*collectionProvider), GetActiveCollection()).WillRepeatedly(testing::ReturnRef(collection));
	const auto databaseProvider = di.Resolve<IDatabaseProvider>();
	setupQuery                  = [](DB::Test::MockQuery& query) {
		EXPECT_CALL(query, BindLong(testing::Matcher<size_t>(testing::_), static_cast<int>(IDatabaseProvider::Key::DatabaseVersion))).WillOnce(testing::Return(0));
		EXPECT_CALL(query, Eof).WillOnce(testing::Return(false));
		EXPECT_CALL(query, GetString(0)).WillOnce(testing::Return(std::string { "specialValue" }));
	};
	EXPECT_EQ(databaseProvider->GetSetting(IDatabaseProvider::Key::DatabaseVersion, QString("defaultValue")).toString(), "specialValue");
}

TEST_F(DatabaseProviderTest, SetSettings)
{
	EXPECT_CALL(*logicFactory, GetExecutor).WillOnce(testing::Return(CreateExecutor()));
	EXPECT_CALL(*collectionProvider, ActiveCollectionExists).WillRepeatedly(testing::Return(true));
	EXPECT_CALL(testing::Const(*collectionProvider), GetActiveCollection()).WillRepeatedly(testing::ReturnRef(collection));
	const auto createCommand = [] {
		auto command = std::make_unique<testing::StrictMock<DB::Test::MockCommand>>();
		EXPECT_CALL(*command, BindLong(testing::Matcher<size_t>(testing::_), static_cast<int>(IDatabaseProvider::Key::DisabledBookFields))).WillOnce(testing::Return(0));
		EXPECT_CALL(*command, BindString(testing::Matcher<size_t>(testing::_), "all")).WillOnce(testing::Return(0));
		EXPECT_CALL(*command, Execute).WillOnce(testing::Return(0));
		return command;
	};
	const auto createTransaction = [createCommand] {
		auto tr = std::make_unique<testing::StrictMock<DB::Test::MockTransaction>>();
		EXPECT_CALL(*tr, CreateCommand).WillOnce(createCommand);
		EXPECT_CALL(*tr, Commit).WillOnce(testing::Return(0));
		return tr;
	};
	setupDatabaseWritable = [createTransaction](DB::Test::MockDatabaseWritable& db) {
		EXPECT_CALL(db, RegisterObserver(testing::_)).WillOnce([](auto*) {
		});
		EXPECT_CALL(db, CreateTransaction).WillOnce(createTransaction);
	};
	const auto databaseProvider = di.Resolve<IDatabaseProvider>();
	databaseProvider->SetSetting(IDatabaseProvider::Key::DisabledBookFields, QVariant("all"));
}

TEST_F(DatabaseProviderTest, Execute)
{
	const auto prepareExecutor = [](MockExecutor& executor) {
		EXPECT_CALL(executor, DoExecute).WillOnce([](IExecutor::Task&& task, int /*priority*/, bool /*displace*/) {
			IExecutor::ThreadContext ctx;
			auto                     callback = task.task(ctx);
			callback(0);
			return 0;
		});
	};
	EXPECT_CALL(*logicFactory, GetExecutor).WillOnce(testing::Return(CreateExecutor(prepareExecutor)));
	EXPECT_CALL(*collectionProvider, ActiveCollectionExists).WillRepeatedly(testing::Return(true));
	EXPECT_CALL(testing::Const(*collectionProvider), GetActiveCollection()).WillRepeatedly(testing::ReturnRef(collection));
	const auto databaseProvider = di.Resolve<IDatabaseProvider>();

	std::latch latch(1);
	databaseProvider->Execute({ "test", [&](DB::IDatabase& db) {
								   const auto db1 = databaseProvider->GetDatabase();
								   EXPECT_EQ(&db, db1.get());
								   return [&](size_t) {
									   latch.count_down();
								   };
							   } });
	latch.wait();
}

TEST_F(DatabaseProviderTest, ExecuteWritable)
{
	const auto prepareExecutor = [](MockExecutor& executor) {
		EXPECT_CALL(executor, DoExecute).WillOnce([](IExecutor::Task&& task, int /*priority*/, bool /*displace*/) {
			IExecutor::ThreadContext ctx;
			auto                     callback = task.task(ctx);
			callback(0);
			return 0;
		});
	};
	EXPECT_CALL(*logicFactory, GetExecutor).WillOnce(testing::Return(CreateExecutor(prepareExecutor)));
	EXPECT_CALL(*collectionProvider, ActiveCollectionExists).WillRepeatedly(testing::Return(true));
	EXPECT_CALL(testing::Const(*collectionProvider), GetActiveCollection()).WillRepeatedly(testing::ReturnRef(collection));

	int observableCount   = 0;
	setupDatabaseWritable = [&](DB::Test::MockDatabaseWritable& db) {
		EXPECT_CALL(db, RegisterObserver(testing::_)).WillOnce([&](auto*) {
			++observableCount;
		});
	};

	const auto databaseProvider = di.Resolve<IDatabaseProvider>();

	std::latch latch(1);
	databaseProvider->ExecuteWritable({ "test", [&](DB::IDatabaseWritable& db) {
										   const auto db1 = databaseProvider->GetDatabaseWritable();
										   EXPECT_NE(&db, db1.get());
										   return [&](size_t) {
											   latch.count_down();
										   };
									   } });
	latch.wait();
	EXPECT_EQ(observableCount, 2);
}

} // namespace
