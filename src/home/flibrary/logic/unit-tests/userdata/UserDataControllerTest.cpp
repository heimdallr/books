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
#include "util/xml/XmlWriter.h"

#include "di.h"

using namespace HomeCompa::DB::Test;
using namespace HomeCompa::DB;
using namespace HomeCompa::Flibrary::Test;
using namespace HomeCompa::Flibrary;
using namespace HomeCompa::Test;
using namespace HomeCompa;

namespace {

constexpr const char* XML[] {
	"FlibraryBackup",
	"FlibraryBackup/FlibraryUserData",
	"FlibraryBackup/FlibraryUserData/Books",
	"FlibraryBackup/FlibraryUserData/ExportStat",
	"FlibraryBackup/FlibraryUserData/Filter",
	"FlibraryBackup/FlibraryUserData/Groups",
	"FlibraryBackup/FlibraryUserData/Searches",
};

struct CheckRestore
{
	QString                           queryText;
	std::vector<std::vector<QString>> results;

	void Check(IDatabase& db) const
	{
		const auto query = db.CreateQuery(queryText);
		if (results.empty())
		{
			ASSERT_TRUE(query->Execute());
			ASSERT_TRUE(query->Eof());
		}
		else
		{
			size_t n = 0;
			for (auto&& [rec, result] : std::views::zip(*query, results))
			{
				++n;
				for (auto&& [value, index] : std::views::zip(result, std::views::iota(0)))
				{
					if (value.isEmpty())
					{
						EXPECT_TRUE(rec.IsNull(index));
					}
					else
					{
						const auto* selected = rec.Get<const char*>(index);
						EXPECT_STREQ(selected, value.toStdString().data());
					}
				}
			}
			EXPECT_EQ(n, results.size());
		}
	}
};

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
	userDataController->Restore([](const bool ok, const QString& errorMessage) {
		EXPECT_FALSE(ok);
		EXPECT_FALSE(errorMessage.isEmpty());
	});
}

TEST_F(UserDataControllerTest, RestoreMHL)
{
	const auto userDataController = di.Resolve<IUserDataController>();
	EXPECT_TRUE(!!userDataController);

	auto db = CreateDatabase<IDatabaseWritable>();
	EXPECT_CALL(*databaseProvider, ExecuteWritable).WillOnce([&](auto&& task, int) {
		auto callback = task.task(*db);
		callback(0);
		return 0;
	});

	EXPECT_CALL(*uiFactory, GetOpenFileName).WillOnce(testing::Return(QString { ":/0001/user_data.mhlud2" }));
	userDataController->Restore([](const bool ok, const QString& /*errorMessage*/) {
		EXPECT_TRUE(ok);
	});

	const CheckRestore checks[] {
		{                                                                                                                      "select IsDeleted, UserRate, Lang from Books_User",
		 {
		 { "", "1", "" },
		 } },
		{ "select g.Title, b.Title from Groups_User g left join Groups_List_User l on l.GroupID = g.GroupID left join Books b on b.BookID = l.ObjectID order by g.Title, b.Title",
		 {
		 { "Favorites", "" },
		 { "To read", "BookTitle" },
		 } },
	};

	for (const auto& check : checks)
		check.Check(*db);
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
				   return result.join('~');
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

struct Node;

struct Node
{
	std::map<QString, QString> attributes;
	std::map<QString, Node>    children;

	Node& Add(const QStringView name)
	{
		const auto sepIndex = name.indexOf('/');
		if (sepIndex < 0)
			return children.try_emplace(name.toString(), decltype(attributes) {}).first->second;

		auto& child = children.try_emplace(QStringView(name.begin(), std::next(name.begin(), sepIndex)).toString(), decltype(attributes) {}).first->second;
		return child.Add(QStringView { std::next(name.begin(), sepIndex + 1), name.end() });
	}
};

struct TestData
{
	std::vector<QString>      setQueries;
	std::set<QString>         xml;
	std::vector<CheckRestore> checkRestore;

	static QByteArray GenerateXml(const std::set<QString>& xml)
	{
		Node root;

		for (const auto& str : xml)
		{
			const auto list = str.split('~');
			auto&      node = root.Add(list.front());
			if (list.size() > 1)
				std::ranges::transform(list.back().split(';'), std::inserter(node.attributes, node.attributes.end()), [](const auto& item) {
					auto itemList = item.split('=', Qt::KeepEmptyParts);
					assert(itemList.size() == 2);
					return std::make_pair(std::move(itemList.front()), std::move(itemList.back()));
				});
		}

		QByteArray result;
		{
			QBuffer stream(&result);
			stream.open(QIODevice::WriteOnly);
			Util::XmlWriter writer(stream);

			const auto enumerate = [&](const Node& parent, const auto& r) -> void {
				for (const auto& [name, node] : parent.children)
				{
					const auto element = writer.Guard(name);
					for (const auto& [key, value] : node.attributes)
						element->WriteAttribute(key, value);
					r(node, r);
				}
			};

			enumerate(root, enumerate);
		}

		return result;
	}
};

namespace {

struct UserDataControllerBackupRestore
    : testing::WithParamInterface<TestData>
    , UserDataControllerTest
{
};

INSTANTIATE_TEST_SUITE_P(
	UserDataControllerTests,
	UserDataControllerBackupRestore,
	testing::Values(
		TestData {
			.checkRestore = { { "select * from Books_User" } },
},
		TestData {
			.setQueries   = { "INSERT INTO Books_User (BookID, IsDeleted, CreatedAt ) VALUES (:bookId, 1, '1974-01-01 02:03:04')" },
			.xml          = { "FlibraryBackup/FlibraryUserData/Books", "FlibraryBackup/FlibraryUserData/Books/Item~CreatedAt=1974-01-01 02:03:04;FileName=100500;Folder=1.zip;IsDeleted=1;Lang=;UserRate=" },
			.checkRestore = { { "select IsDeleted, UserRate, Lang from Books_User", { { "1", "", "" } } } },
		},
		TestData {
			.setQueries   = { "INSERT INTO Books_User (BookID, UserRate, CreatedAt ) VALUES (:bookId, 3, '1974-01-01 02:03:05')" },
			.xml          = { "FlibraryBackup/FlibraryUserData/Books", "FlibraryBackup/FlibraryUserData/Books/Item~CreatedAt=1974-01-01 02:03:05;FileName=100500;Folder=1.zip;IsDeleted=;Lang=;UserRate=3" },
			.checkRestore = { { "select IsDeleted, UserRate, Lang from Books_User", { { "", "3", "" } } } },
		},
		TestData {
			.setQueries   = { "INSERT INTO Books_User (BookID, Lang, CreatedAt ) VALUES (:bookId, 'ja', '1974-01-01 02:03:06')" },
			.xml          = { "FlibraryBackup/FlibraryUserData/Books", "FlibraryBackup/FlibraryUserData/Books/Item~CreatedAt=1974-01-01 02:03:06;FileName=100500;Folder=1.zip;IsDeleted=;Lang=ja;UserRate=" },
			.checkRestore = { { "select IsDeleted, UserRate, Lang from Books_User", { { "", "", "ja" } } } },
		},
		TestData {
			.setQueries   = { "INSERT INTO Books_User (BookID, IsDeleted, UserRate, Lang, CreatedAt ) VALUES (:bookId, 0, 5, 'uk', '1974-01-01 02:03:07')" },
			.xml          = { "FlibraryBackup/FlibraryUserData/Books", "FlibraryBackup/FlibraryUserData/Books/Item~CreatedAt=1974-01-01 02:03:07;FileName=100500;Folder=1.zip;IsDeleted=0;Lang=uk;UserRate=5" },
			.checkRestore = { { "select IsDeleted, UserRate, Lang from Books_User", { { "0", "5", "uk" } } } },
		},
		TestData {
			.setQueries   = { "insert into Groups_User(Title, CreatedAt) values('My books group title', '1974-01-01 02:03:08')",
			                  "insert into Groups_List_User(GroupID, ObjectID, CreatedAt) values(1, :bookId, '1974-01-01 02:03:09')" },
			.xml          = { "FlibraryBackup/FlibraryUserData/Groups/Group/Item~CreatedAt=1974-01-01 02:03:09;FileName=100500;Folder=1.zip",
			                  "FlibraryBackup/FlibraryUserData/Groups/Group~CreatedAt=1974-01-01 02:03:08;Title=My books group title" },
			.checkRestore = { { "select Title from Groups_User", { { "My books group title" } } }, { "select count(42) from Groups_List_User l join Books b on b.BookID = l.ObjectID", { { "1" } } } },
		},
		TestData {
			.setQueries   = { "insert into Groups_User(Title, CreatedAt) values('My authors group title', '1974-01-01 02:03:10')",
			                  "insert into Groups_List_User(GroupID, ObjectID, CreatedAt) values(1, :authorId, '1974-01-01 02:03:11')" },
			.xml          = { "FlibraryBackup/FlibraryUserData/Groups/Group/Author~CreatedAt=1974-01-01 02:03:11;Title=LastName,FirstName,MiddleName",
			                  "FlibraryBackup/FlibraryUserData/Groups/Group~CreatedAt=1974-01-01 02:03:10;Title=My authors group title" },
			.checkRestore = { { "select Title from Groups_User", { { "My authors group title" } } }, { "select count(42) from Groups_List_User l join Authors a on a.AuthorID = l.ObjectID", { { "1" } } } },
		},
		TestData {
			.setQueries   = { "insert into Groups_User(Title, CreatedAt) values('My series group title', '1974-01-01 02:03:12')",
			                  "insert into Groups_List_User(GroupID, ObjectID, CreatedAt) values(1, :seriesId, '1974-01-01 02:03:13')" },
			.xml          = { "FlibraryBackup/FlibraryUserData/Groups/Group/Series~CreatedAt=1974-01-01 02:03:13;Title=SeriesTitle",
			                  "FlibraryBackup/FlibraryUserData/Groups/Group~CreatedAt=1974-01-01 02:03:12;Title=My series group title" },
			.checkRestore = { { "select Title from Groups_User", { { "My series group title" } } }, { "select count(42) from Groups_List_User l join Series s on s.SeriesID = l.ObjectID", { { "1" } } } },
		},
		TestData {
			.setQueries   = { "insert into Searches_User(Origin, Title, CreatedAt) values('horse', 'horse*', '1974-01-01 02:03:14')" },
			.xml          = { "FlibraryBackup/FlibraryUserData/Searches/Item~CreatedAt=1974-01-01 02:03:14;Origin=horse;Title=horse*" },
			.checkRestore = { { "select Origin, Title from Searches_User", { { "horse", "horse*" } } } },
		},
		TestData {
			.setQueries   = { "insert into Export_List_User(BookID, ExportType, CreatedAt) values(:bookId, 4, '1974-01-01 02:03:15')" },
			.xml          = { "FlibraryBackup/FlibraryUserData/ExportStat/Item~CreatedAt=1974-01-01 02:03:15;ExportType=4;FileName=100500;Folder=1.zip" },
			.checkRestore = { { "select ExportType from Export_List_User", { { "4" } } } },
		},
		TestData {
			.setQueries   = { "update Languages set Flags = 1 where LanguageCode = 'sq'" },
			.xml          = { "FlibraryBackup/FlibraryUserData/Filter/Languages", "FlibraryBackup/FlibraryUserData/Filter/Languages/Item~Flag=1;Title=sq" },
			.checkRestore = { { "select LanguageCode, Flags from Languages", { { "sq", "1" } } } },
		},
		TestData {
			.setQueries   = { "update Authors set Flags = 3 where AuthorID = :authorId" },
			.xml          = { "FlibraryBackup/FlibraryUserData/Filter/Authors", "FlibraryBackup/FlibraryUserData/Filter/Authors/Item~Flag=3;Title=LastName,FirstName,MiddleName" },
			.checkRestore = { { "select LastName, FirstName, MiddleName, Flags from Authors", { { "LastName", "FirstName", "MiddleName", "3" } } } },
		},
		TestData {
			.setQueries   = { "update Series set Flags = 2 where SeriesID = :seriesId" },
			.xml          = { "FlibraryBackup/FlibraryUserData/Filter/Series", "FlibraryBackup/FlibraryUserData/Filter/Series/Item~Flag=2;Title=SeriesTitle" },
			.checkRestore = { { "select SeriesTitle, Flags from Series", { { "SeriesTitle", "2" } } } },
		},
		TestData {
			.setQueries   = { "update Genres set Flags = 2 where FB2Code='sf'" },
			.xml          = { "FlibraryBackup/FlibraryUserData/Filter/Genres", "FlibraryBackup/FlibraryUserData/Filter/Genres/Item~Flag=2;Title=sf" },
			.checkRestore = { { "select FB2Code, Flags from Genres where Flags <> 0", { { "sf", "2" } } } },
		},
		TestData {
			.setQueries   = { "update Keywords set Flags = 1 where KeywordID = :keywordId" },
			.xml          = { "FlibraryBackup/FlibraryUserData/Filter/Keywords", "FlibraryBackup/FlibraryUserData/Filter/Keywords/Item~Flag=1;Title=Story" },
			.checkRestore = { { "select KeywordTitle, Flags from Keywords", { { "Story", "1" } } } },
		}
	)
);

}

TEST_P(UserDataControllerBackupRestore, Backup)
{
	const auto& [setQueries, checkSetSrc, _] = GetParam();

	auto checkSet = XML | std::ranges::to<decltype(TestData::xml)>();
	checkSet.emplace(QString("FlibraryBackup/FlibraryBackupVersion~Value=%1").arg(Constant::FlibraryBackupVersionNumber));
	std::ranges::copy(checkSetSrc, std::inserter(checkSet, checkSet.end()));

	QByteArray bytes;
	{
		QBuffer stream(&bytes);
		ASSERT_TRUE(stream.open(QIODevice::WriteOnly));

		const auto getId = [](IDatabase& db, const std::string_view queryText) {
			const auto query = db.CreateQuery(queryText);
			query->Execute();
			[&] {
				ASSERT_FALSE(query->Eof());
			}();
			return query->Get<long long>(0);
		};

		const auto database = [&] {
			std::tuple<const char*, const char*, long long> ids[] {
				{    ":bookId",       "select max(BookId) from Books", -1 },
				{  ":authorId",   "select max(AuthorId) from Authors", -1 },
				{  ":seriesId",    "select max(SeriesId) from Series", -1 },
				{ ":keywordId", "select max(KeywordId) from Keywords", -1 },
			};
			auto db = CreateDatabase<IDatabaseWritable>();
			for (auto& [_, queryText, id] : ids)
				id = getId(*db, queryText);

			const auto tr = db->CreateTransaction();
			for (const auto& queryText : setQueries)
			{
				const auto query = tr->CreateQuery(queryText);
				for (auto& [placeholder, _, id] : ids)
					if (queryText.contains(placeholder))
						query->Bind(placeholder, id);
				query->Execute();
			}
			tr->Commit();
			return db;
		}();

		EXPECT_CALL(*databaseProvider, Execute).WillOnce([&](auto&& task, int) {
			auto callback = task.task(*database);
			callback(0);
			return 0;
		});

		const auto userDataController = di.Resolve<IUserDataController>();
		ASSERT_TRUE(!!userDataController);
		userDataController->Backup(stream, [&](const bool ok, const QString& /*errorMessage*/) {
			EXPECT_TRUE(ok);
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

TEST_P(UserDataControllerBackupRestore, RestoreActual)
{
	const auto& [_, xmlSrc, checkSet] = GetParam();
	auto xml                          = XML | std::ranges::to<decltype(TestData::xml)>();
	xml.emplace(QString("FlibraryBackup/FlibraryBackupVersion~Value=%1").arg(Constant::FlibraryBackupVersionNumber));
	std::ranges::copy(xmlSrc, std::inserter(xml, xml.end()));
	auto bytes = TestData::GenerateXml(xml);

	auto db = CreateDatabase<IDatabaseWritable>();

	QBuffer stream(&bytes);
	ASSERT_TRUE(stream.open(QIODevice::ReadOnly));

	const auto userDataController = di.Resolve<IUserDataController>();
	ASSERT_TRUE(!!userDataController);

	EXPECT_CALL(*databaseProvider, ExecuteWritable).WillOnce([&](auto&& task, int) {
		auto callback = task.task(*db);
		callback(0);
		return 0;
	});

	userDataController->Restore(stream, IUserDataController::RestoreMode::FLibrary, [](const bool ok, const QString& /*errorMessage*/) {
		EXPECT_TRUE(ok);
	});

	for (const auto& check : checkSet)
		check.Check(*db);
}

} // namespace
