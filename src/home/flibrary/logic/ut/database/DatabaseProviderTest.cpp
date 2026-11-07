#include <gtest/gtest.h>

#include "interface/logic/IDatabaseProvider.h"

#include "Hypodermic/Hypodermic.h"
#include "mock/logic/MockCollectionProvider.h"
#include "mock/logic/MockLogicFactory.h"
#include "mock/util/MockExecutor.h"

#include "di.h"

namespace HomeCompa::Flibrary::Test {

struct CollectionTest : Collection
{
	CollectionTest()
	{
		id = "testId";
		name = "testName";
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

namespace {

struct DatabaseProviderTest : testing::Test
{
	CollectionTest                          collection;
	std::shared_ptr<MockLogicFactory>       logicFactory { std::make_shared<MockLogicFactory>() };
	std::shared_ptr<MockCollectionProvider> collectionProvider { std::make_shared<MockCollectionProvider>() };

	DI di { [this](Hypodermic::ContainerBuilder& builder, Hypodermic::Container& /*container*/) {
		builder.registerInstance(collectionProvider).as<ICollectionProvider>();
		builder.registerInstance(logicFactory).as<ILogicFactory>();
	} };
};

std::unique_ptr<IExecutor> CreateExecutor()
{
	auto executor = std::make_unique<MockExecutor>();
	return executor;
}

TEST_F(DatabaseProviderTest, CreateWithoutActiveCollection)
{
	EXPECT_CALL(*logicFactory, GetExecutor).WillOnce(testing::Return(CreateExecutor()));
	EXPECT_CALL(*collectionProvider, ActiveCollectionExists).Times(2).WillRepeatedly(testing::Return(false));
	const auto databaseProvider = di.Resolve<IDatabaseProvider>();
	EXPECT_TRUE(!!databaseProvider);
	EXPECT_FALSE(databaseProvider->IsDatabaseValid());
}

TEST_F(DatabaseProviderTest, Create)
{
	EXPECT_CALL(*logicFactory, GetExecutor).WillOnce(testing::Return(CreateExecutor()));
	EXPECT_CALL(*collectionProvider, ActiveCollectionExists).Times(2).WillRepeatedly(testing::Return(true));
	EXPECT_CALL(testing::Const(*collectionProvider), GetActiveCollection()).Times(2).WillRepeatedly(testing::ReturnRef(collection));
	const auto databaseProvider = di.Resolve<IDatabaseProvider>();
	EXPECT_TRUE(!!databaseProvider);
	EXPECT_TRUE(databaseProvider->IsDatabaseValid());
}

} // namespace
