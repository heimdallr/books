#include <QBuffer>

#include <gtest/gtest.h>

#include "interface/constants/ProductConstant.h"
#include "interface/logic/IDatabaseProvider.h"
#include "interface/logic/IUserDataController.h"

#include "database/mock/MockDatabase.h"
#include "mock/logic/MockDatabaseProvider.h"
#include "mock/ui/MockUiFactory.h"
#include "unit-test-util/database.h"
#include "util/xml/SaxParser.h"
#include "util/xml/XmlAttributes.h"

#include "di.h"

using namespace HomeCompa::DB::Test;
using namespace HomeCompa::DB;
using namespace HomeCompa::Flibrary::Test;
using namespace HomeCompa::Flibrary;
using namespace HomeCompa::Test;
using namespace HomeCompa;

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
	//	EXPECT_CALL(*databaseProvider, ExecuteWritable).WillOnce([](auto&& task, int) {
	//		MockDatabseWritable db;
	//		auto        callback = task.task(db);
	//		callback(0);
	//		return 0;
	//	});

	userDataController->Restore([](const bool ok, const QString& errorMessage) {
		EXPECT_FALSE(ok);
		EXPECT_FALSE(errorMessage.isEmpty());
	});
}

struct XmlParser final : Util::SaxParser
{
	explicit XmlParser(QIODevice& stream)
		: SaxParser(stream)
	{
		Parse();
	}

	[[nodiscard]] std::set<QString> GetResult() const
	{
		return m_data | std::views::transform([](const auto& item) {
				   QStringList result = { item.first };
				   if (!item.second.empty())
					   result << (item.second | std::views::transform([](const auto& subItem) {
									  return QString("%1=%2").arg(subItem.first, subItem.second);
								  })
				                  | std::ranges::to<QStringList>())
									 .join(';');
				   return result.join(':');
			   })
		     | std::views::common | std::ranges::to<std::set>();
	}

private: // SaxParser
	bool OnStartElement(QStringView, const QStringView path, const Util::XmlAttributes& attributes) override
	{
		auto& values = m_data.emplace(path.toString(), std::map<QString, QString> {})->second;
		for (size_t i = 0, sz = attributes.GetCount(); i < sz; ++i)
			values.try_emplace(attributes.GetName(i).toString(), attributes.GetValue(i).toString());
		return true;
	}

private:
	std::unordered_multimap<QString, std::map<QString, QString>> m_data;
};

struct TestData
{
	std::vector<QString> queries;
	std::set<QString>    result;
};

namespace {

struct UserDataControllerBackup
    : testing::WithParamInterface<TestData>
    , UserDataControllerTest
{
};

INSTANTIATE_TEST_SUITE_P(
	UserDataControllerBackups,
	UserDataControllerBackup,
	testing::Values(
		TestData {
			.queries = {},
			.result  = { "FlibraryBackup",
			            QString("FlibraryBackup/FlibraryBackupVersion:Value=%1").arg(Constant::FlibraryBackupVersionNumber),
			            "FlibraryBackup/FlibraryUserData", "FlibraryBackup/FlibraryUserData/Books",
			            "FlibraryBackup/FlibraryUserData/ExportStat", "FlibraryBackup/FlibraryUserData/Filter",
			            "FlibraryBackup/FlibraryUserData/Groups", "FlibraryBackup/FlibraryUserData/Searches" }
},
		TestData { .queries = { "INSERT INTO Books_User (BookID, IsDeleted, CreatedAt ) VALUES (?, 1, '1974-01-01 02:03:04' )" },
		           .result  = { "FlibraryBackup",
		                        QString("FlibraryBackup/FlibraryBackupVersion:Value=%1").arg(Constant::FlibraryBackupVersionNumber),
		                        "FlibraryBackup/FlibraryUserData",
		                        "FlibraryBackup/FlibraryUserData/Books",
		                        "FlibraryBackup/FlibraryUserData/Books/Item:CreatedAt=1974-01-01 02:03:04;FileName=100500;Folder=1.zip;IsDeleted=1;Lang=;UserRate=",
		                        "FlibraryBackup/FlibraryUserData/ExportStat",
		                        "FlibraryBackup/FlibraryUserData/Filter",
		                        "FlibraryBackup/FlibraryUserData/Groups",
		                        "FlibraryBackup/FlibraryUserData/Searches" } },
		TestData { .queries = { "INSERT INTO Books_User (BookID, UserRate, CreatedAt ) VALUES (?, 3, '1974-01-01 02:03:05' )" },
		           .result  = { "FlibraryBackup",
		                        QString("FlibraryBackup/FlibraryBackupVersion:Value=%1").arg(Constant::FlibraryBackupVersionNumber),
		                        "FlibraryBackup/FlibraryUserData",
		                        "FlibraryBackup/FlibraryUserData/Books",
		                        "FlibraryBackup/FlibraryUserData/Books/Item:CreatedAt=1974-01-01 02:03:05;FileName=100500;Folder=1.zip;IsDeleted=;Lang=;UserRate=3",
		                        "FlibraryBackup/FlibraryUserData/ExportStat",
		                        "FlibraryBackup/FlibraryUserData/Filter",
		                        "FlibraryBackup/FlibraryUserData/Groups",
		                        "FlibraryBackup/FlibraryUserData/Searches" } },
		TestData { .queries = { "INSERT INTO Books_User (BookID, Lang, CreatedAt ) VALUES (?, 'ja', '1974-01-01 02:03:06' )" },
		           .result  = { "FlibraryBackup",
		                        QString("FlibraryBackup/FlibraryBackupVersion:Value=%1").arg(Constant::FlibraryBackupVersionNumber),
		                        "FlibraryBackup/FlibraryUserData",
		                        "FlibraryBackup/FlibraryUserData/Books",
		                        "FlibraryBackup/FlibraryUserData/Books/Item:CreatedAt=1974-01-01 02:03:06;FileName=100500;Folder=1.zip;IsDeleted=;Lang=ja;UserRate=",
		                        "FlibraryBackup/FlibraryUserData/ExportStat",
		                        "FlibraryBackup/FlibraryUserData/Filter",
		                        "FlibraryBackup/FlibraryUserData/Groups",
		                        "FlibraryBackup/FlibraryUserData/Searches" } },
		TestData { .queries = { "INSERT INTO Books_User (BookID, IsDeleted, UserRate, Lang, CreatedAt ) VALUES (?, 0, 5, 'uk', '1974-01-01 02:03:07' )" },
		           .result  = { "FlibraryBackup",
		                        QString("FlibraryBackup/FlibraryBackupVersion:Value=%1").arg(Constant::FlibraryBackupVersionNumber),
		                        "FlibraryBackup/FlibraryUserData",
		                        "FlibraryBackup/FlibraryUserData/Books",
		                        "FlibraryBackup/FlibraryUserData/Books/Item:CreatedAt=1974-01-01 02:03:07;FileName=100500;Folder=1.zip;IsDeleted=0;Lang=uk;UserRate=5",
		                        "FlibraryBackup/FlibraryUserData/ExportStat",
		                        "FlibraryBackup/FlibraryUserData/Filter",
		                        "FlibraryBackup/FlibraryUserData/Groups",
		                        "FlibraryBackup/FlibraryUserData/Searches" } }
	)
);

}

TEST_P(UserDataControllerBackup, Backup)
{
	const auto& [queries, checkSet] = GetParam();

	const auto userDataController = di.Resolve<IUserDataController>();
	EXPECT_TRUE(!!userDataController);

	QByteArray bytes;
	{
		QBuffer stream(&bytes);
		ASSERT_TRUE(stream.open(QIODevice::WriteOnly));

		const auto database = [&] {
			auto       db     = CreateDatabase<IDatabaseWritable>();
			const auto bookId = [&] {
				const auto query = db->CreateQuery("select max(BookId) from Books");
				query->Execute();
				[&] {
					ASSERT_FALSE(query->Eof());
				}();
				return query->Get<long long>(0);
			}();

			const auto tr = db->CreateTransaction();
			for (const auto& query : queries)
				tr->CreateQuery(query)->Execute(bookId);
			tr->Commit();
			return db;
		}();

		EXPECT_CALL(*databaseProvider, Execute).WillOnce([&](auto&& task, int) {
			auto callback = task.task(*database);
			callback(0);
			return 0;
		});

		userDataController->Backup(stream, [&](const bool ok, const QString& errorMessage) {
			EXPECT_TRUE(ok);
			EXPECT_FALSE(errorMessage.isEmpty());
			stream.close();
		});
	}

	QBuffer stream(&bytes);
	ASSERT_TRUE(stream.open(QIODevice::ReadOnly));
	const XmlParser parser(stream);
	const auto      serializedSet = parser.GetResult();

	EXPECT_EQ(serializedSet.size(), checkSet.size());
	for (auto&& [serialized, check] : std::views::zip(serializedSet, checkSet))
		EXPECT_STREQ(serialized.toStdString().data(), check.toStdString().data());
}

} // namespace
