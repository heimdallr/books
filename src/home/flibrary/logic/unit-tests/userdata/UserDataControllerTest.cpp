#include <gtest/gtest.h>

#include "interface/logic/IDatabaseProvider.h"
#include "interface/logic/IUserDataController.h"

#include "database/mock/MockDatabase.h"
#include "mock/logic/MockDatabaseProvider.h"
#include "mock/ui/MockUiFactory.h"

#include "di.h"

using namespace HomeCompa::Flibrary::Test;
using namespace HomeCompa::Flibrary;
using namespace HomeCompa::DB::Test;

namespace {

struct UserDataControllerTest : testing::Test
{
	std::shared_ptr<MockDatabaseProvider> databaseProvider { std::make_shared<testing::StrictMock<MockDatabaseProvider>>() };
	std::shared_ptr<MockUiFactory>        uiFactory { std::make_shared<testing::StrictMock<MockUiFactory>>() };

	DI di { [this](Hypodermic::ContainerBuilder& builder, Hypodermic::Container& /*container*/) {
		builder.registerInstance(databaseProvider).as<IDatabaseProvider>();
		builder.registerInstance(uiFactory).as<IUiFactory>();
	} };
};

TEST_F(UserDataControllerTest, BackupCancelled)
{
	const auto userDataController = di.Resolve<IUserDataController>();
	EXPECT_TRUE(!!userDataController);

	EXPECT_CALL(*uiFactory, GetSaveFileName).WillOnce(testing::Return(QString {}));
	userDataController->Backup([](const bool ok, const QString& errorMessage) {
		EXPECT_FALSE(ok);
		EXPECT_TRUE(errorMessage.isEmpty());
	});
}

TEST_F(UserDataControllerTest, RestoreCancelled)
{
	const auto userDataController = di.Resolve<IUserDataController>();
	EXPECT_TRUE(!!userDataController);

	EXPECT_CALL(*uiFactory, GetOpenFileName).WillOnce(testing::Return(QString {}));
	userDataController->Restore([](const bool ok, const QString& errorMessage) {
		EXPECT_FALSE(ok);
		EXPECT_TRUE(errorMessage.isEmpty());
	});
}

TEST_F(UserDataControllerTest, BackupWithBadFileName)
{
	const auto userDataController = di.Resolve<IUserDataController>();
	EXPECT_TRUE(!!userDataController);

	EXPECT_CALL(*uiFactory, GetSaveFileName).WillOnce(testing::Return(QString { "fake/File\\Name" }));
	EXPECT_CALL(*databaseProvider, Execute).WillOnce([](auto&& task, int) {
		MockDatabse db;
		auto        callback = task.task(db);
		callback(0);
		return 0;
	});

	userDataController->Backup([](const bool ok, const QString& errorMessage) {
		EXPECT_FALSE(ok);
		EXPECT_FALSE(errorMessage.isEmpty());
	});
}

TEST_F(UserDataControllerTest, RestoreWithBadFileName)
{
	const auto userDataController = di.Resolve<IUserDataController>();
	EXPECT_TRUE(!!userDataController);

	EXPECT_CALL(*uiFactory, GetOpenFileName).WillOnce(testing::Return(QString { "fake/File\\Name" }));
	EXPECT_CALL(*databaseProvider, ExecuteWritable).WillOnce([](auto&& task, int) {
		MockDatabseWritable db;
		auto        callback = task.task(db);
		callback(0);
		return 0;
	});

	userDataController->Restore([](const bool ok, const QString& errorMessage) {
		EXPECT_FALSE(ok);
		EXPECT_FALSE(errorMessage.isEmpty());
	});
}

} // namespace
