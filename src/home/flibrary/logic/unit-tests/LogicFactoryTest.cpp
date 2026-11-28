#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "database/interface/IDatabase.h"

#include "interface/constants/ModelRole.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IScriptController.h"

#include "database/mock/MockCommand.h"
#include "database/mock/MockDatabase.h"
#include "database/mock/MockQuery.h"
#include "database/mock/MockTransaction.h"
#include "gutil/mock/MockParentWidgetProvider.h"
#include "mock/logic/MockAbstractItemModel.h"
#include "mock/logic/MockAnnotationController.h"
#include "mock/logic/MockAuthorAnnotationController.h"
#include "mock/logic/MockBookExtractor.h"
#include "mock/logic/MockBookInteractor.h"
#include "mock/logic/MockBookSearchController.h"
#include "mock/logic/MockCollectionCleaner.h"
#include "mock/logic/MockCollectionController.h"
#include "mock/logic/MockCollectionProvider.h"
#include "mock/logic/MockDataProvider.h"
#include "mock/logic/MockDatabaseController.h"
#include "mock/logic/MockDatabaseProvider.h"
#include "mock/logic/MockFilterController.h"
#include "mock/logic/MockMenuCustomizer.h"
#include "mock/logic/MockModelProvider.h"
#include "mock/logic/MockNavigationQueryExecutor.h"
#include "mock/logic/MockOpdsController.h"
#include "mock/logic/MockProgressController.h"
#include "mock/logic/MockReaderController.h"
#include "mock/logic/MockScriptController.h"
#include "mock/logic/MockTreeViewController.h"
#include "mock/logic/MockUpdateChecker.h"
#include "mock/logic/MockUserDataController.h"
#include "mock/ui/MockUiFactory.h"
#include "mock/util/MockSettings.h"
#include "unit-test-util/book.h"
#include "unit-test-util/database.h"

#include "di.h"

namespace {

namespace FillScriptTemplateTest {

using namespace HomeCompa::DB;
using namespace HomeCompa::Flibrary;
using namespace HomeCompa::Test;
using namespace HomeCompa::Util;

constexpr auto SourceFile            = "";
constexpr auto UserDestinationFolder = "";
constexpr auto Title                 = "BookTitle";
constexpr auto FileName              = "100500.fb2";
constexpr auto FileExt               = "fb2";
constexpr auto BaseFileName          = "100500";
constexpr auto Author                = "LastName FirstName MiddleName";
constexpr auto AuthorLastFM          = "LastName F.M.";
constexpr auto AuthorLastName        = "LastName";
constexpr auto AuthorFirstName       = "FirstName";
constexpr auto AuthorMiddleName      = "MiddleName";
constexpr auto AuthorF               = "F";
constexpr auto AuthorM               = "M";
constexpr auto Series                = "SeriesTitle";
constexpr auto SeqNumber             = "42";
constexpr auto FileSize              = "2001000";
constexpr auto Genre                 = "";
constexpr auto GenreTree             = "";
constexpr auto Keyword               = "Story";
constexpr auto AllKeywords           = "Story";
constexpr auto Language              = "sq";
constexpr auto Id                    = "";
constexpr auto LibId                 = "100500";
constexpr auto Uid                   = "";

template <IScriptController::Macro T>
void Check(const QString& fileNameTemplate, const ExtractedBook&, const char* result)
{
	EXPECT_STREQ(fileNameTemplate.toUtf8().constData(), result);
}

template <>
void Check<IScriptController::Macro::Genre>(const QString& fileNameTemplate, const ExtractedBook& book, const char*)
{
	EXPECT_EQ(fileNameTemplate, book.genre);
}

template <>
void Check<IScriptController::Macro::GenreTree>(const QString& fileNameTemplate, const ExtractedBook& book, const char*)
{
	EXPECT_EQ(fileNameTemplate, book.genreTree.join('/'));
}

template <>
void Check<IScriptController::Macro::Id>(const QString& fileNameTemplate, const ExtractedBook& book, const char*)
{
	EXPECT_EQ(fileNameTemplate.toLongLong(), book.id);
}

template <>
void Check<IScriptController::Macro::Uid>(const QString& fileNameTemplate, const ExtractedBook&, const char* result)
{
	EXPECT_STRNE(fileNameTemplate.toUtf8().constData(), result);
}

template <IScriptController::Macro T>
void Test(IDatabase& db, QString fileNameTemplate, const ExtractedBook& book, const char* result)
{
	ILogicFactory::FillScriptTemplate(db, fileNameTemplate, book);
	Check<T>(fileNameTemplate, book, result);
}

TEST(LogicFactoryTestStatic, FillScriptTemplate)
{
	const auto db = CreateDatabase<IDatabase>();
	ASSERT_TRUE(!!db);
	const auto books = GetExtractedBooks(*db);
	ASSERT_FALSE(books.empty());
	const auto& book = books.front();
	QString     fileName;
	FillScriptTemplateTest::Test<static_cast<IScriptController::Macro>(-1)>(*db, fileName, book, "");

#define SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(NAME) { QString f = IScriptController::s_commandMacros[static_cast<size_t>(IScriptController::Macro::NAME)].second; FillScriptTemplateTest::Test<IScriptController::Macro::NAME>(*db, f, book, FillScriptTemplateTest::NAME); }
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEMS_X_MACRO
#undef SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM
}

} // namespace FillScriptTemplateTest

using namespace HomeCompa::DB::Test;
using namespace HomeCompa::Util::Test;
using namespace HomeCompa::Flibrary::Test;
using namespace HomeCompa::Flibrary;
using namespace HomeCompa::Test;
using namespace HomeCompa;

#define LOGIC_FACTORY_TEST_MOCK_ITEMS_X_MACRO             \
LOGIC_FACTORY_TEST_MOCK_ITEM(UiFactory)                   \
LOGIC_FACTORY_TEST_MOCK_ITEM(ParentWidgetProvider)        \
LOGIC_FACTORY_TEST_MOCK_ITEM(Settings)                    \
LOGIC_FACTORY_TEST_MOCK_ITEM(ModelProvider)               \
LOGIC_FACTORY_TEST_MOCK_ITEM(DatabaseController)          \
LOGIC_FACTORY_TEST_MOCK_ITEM(DatabaseProvider)            \
LOGIC_FACTORY_TEST_MOCK_ITEM(CollectionCleaner)           \
LOGIC_FACTORY_TEST_MOCK_ITEM(CollectionController)        \
LOGIC_FACTORY_TEST_MOCK_ITEM(CollectionProvider)          \
LOGIC_FACTORY_TEST_MOCK_ITEM(MenuCustomizer)              \
LOGIC_FACTORY_TEST_MOCK_ITEM(NavigationInfoProvider)      \
LOGIC_FACTORY_TEST_MOCK_ITEM(BookInfoProvider)            \
LOGIC_FACTORY_TEST_MOCK_ITEM(AnnotationController)        \
LOGIC_FACTORY_TEST_MOCK_ITEM(AuthorAnnotationController)  \
LOGIC_FACTORY_TEST_MOCK_ITEM(FilterController)            \
LOGIC_FACTORY_TEST_MOCK_ITEM(BookExtractor)               \
LOGIC_FACTORY_TEST_MOCK_ITEM(BookInteractor)              \
LOGIC_FACTORY_TEST_MOCK_ITEM(ProgressController)          \
LOGIC_FACTORY_TEST_MOCK_ITEM(ReaderController)            \
LOGIC_FACTORY_TEST_MOCK_ITEM(ScriptController)            \
LOGIC_FACTORY_TEST_MOCK_ITEM(NavigationQueryExecutor)     \
LOGIC_FACTORY_TEST_MOCK_ITEM(BookSearchController)        \
LOGIC_FACTORY_TEST_MOCK_ITEM(UserDataController)          \
LOGIC_FACTORY_TEST_MOCK_ITEM(UpdateChecker)               \
LOGIC_FACTORY_TEST_MOCK_ITEM(OpdsController)

struct LogicFactoryTest : testing::Test
{
#define LOGIC_FACTORY_TEST_MOCK_ITEM(NAME) std::shared_ptr<testing::StrictMock<Mock##NAME>> m_##NAME = std::make_shared<testing::StrictMock<Mock##NAME>>();
	LOGIC_FACTORY_TEST_MOCK_ITEMS_X_MACRO
#undef LOGIC_FACTORY_TEST_MOCK_ITEM

	DI di { [this](Hypodermic::ContainerBuilder& builder, Hypodermic::Container& /*container*/) {
#define LOGIC_FACTORY_TEST_MOCK_ITEM(NAME) builder.registerInstance(m_##NAME).as<I##NAME>();
		LOGIC_FACTORY_TEST_MOCK_ITEMS_X_MACRO
#undef LOGIC_FACTORY_TEST_MOCK_ITEM
		builder.registerInstance(m_ProgressController).as<IMainProgressController>();
	} };

	std::shared_ptr<ILogicFactory> logicFactory { di.Resolve<ILogicFactory>() };

	LogicFactoryTest()
	{
		EXPECT_TRUE(!!logicFactory);
	}

	template <typename T>
	static std::unique_ptr<testing::StrictMock<T>> CreateDatabase()
	{
		return std::make_unique<testing::StrictMock<T>>();
	}

	static std::unique_ptr<testing::StrictMock<MockTransaction>> CreateTransaction()
	{
		return std::make_unique<testing::StrictMock<MockTransaction>>();
	}

	static std::unique_ptr<testing::StrictMock<MockCommand>> CreateCommand()
	{
		return std::make_unique<testing::StrictMock<MockCommand>>();
	}

	static std::unique_ptr<testing::StrictMock<MockQuery>> CreateQuery()
	{
		return std::make_unique<testing::StrictMock<MockQuery>>();
	}
};

TEST_F(LogicFactoryTest, GetTreeViewControllerFail)
{
	const auto fail = [&](const ItemType itemType) {
		std::ignore = logicFactory->GetTreeViewController(itemType);
	};
	EXPECT_DEBUG_DEATH(fail(ItemType::Unknown), "");
	EXPECT_DEBUG_DEATH(fail(ItemType::Last), "");
}

TEST_F(LogicFactoryTest, GetTreeViewControllerNavigation)
{
	EXPECT_CALL(*m_MenuCustomizer, IsHidden).WillRepeatedly(testing::Return(true));
	EXPECT_CALL(*m_DatabaseController, RegisterObserver);
	EXPECT_CALL(*m_CollectionProvider, ActiveCollectionExists).WillOnce(testing::Return(false));
	EXPECT_TRUE(!!logicFactory->GetTreeViewController(ItemType::Navigation));
	EXPECT_TRUE(!!logicFactory->GetTreeViewController(ItemType::Navigation));
}

TEST_F(LogicFactoryTest, GetTreeViewControllerBooks)
{
	EXPECT_CALL(*m_Settings, Get(testing::_, testing::Matcher<const QVariant&>(testing::_))).WillOnce(testing::Return(false)).WillOnce(testing::Return(QString {}));
	EXPECT_CALL(*m_BookInfoProvider, SetBooksViewMode);
	EXPECT_CALL(*m_BookInfoProvider, SetBookRequestCallback);
	EXPECT_CALL(*m_BookInfoProvider, RegisterObserver);
	EXPECT_TRUE(!!logicFactory->GetTreeViewController(ItemType::Books));
	EXPECT_TRUE(!!logicFactory->GetTreeViewController(ItemType::Books));
}

TEST_F(LogicFactoryTest, CreateArchiveParser)
{
	EXPECT_TRUE(!!logicFactory->CreateArchiveParser());
}

TEST_F(LogicFactoryTest, GetExecutor)
{
	EXPECT_TRUE(!!logicFactory->GetExecutor());
}

TEST_F(LogicFactoryTest, CreateGroupController)
{
	EXPECT_TRUE(!!logicFactory->CreateGroupController());
}

TEST_F(LogicFactoryTest, CreateSearchController)
{
	EXPECT_TRUE(!!logicFactory->CreateSearchController());
}

TEST_F(LogicFactoryTest, CreateBooksContextMenuProvider)
{
	EXPECT_CALL(*m_Settings, Get(testing::_, testing::Matcher<const QVariant&>(testing::_))).WillOnce(testing::Return(0));
	EXPECT_TRUE(!!logicFactory->CreateBooksContextMenuProvider());
}

TEST_F(LogicFactoryTest, CreateUserDataController)
{
	EXPECT_TRUE(!!logicFactory->CreateUserDataController());
}

TEST_F(LogicFactoryTest, CreateBooksExtractor)
{
	EXPECT_CALL(*m_Settings, Get(testing::_, testing::Matcher<const QVariant&>(testing::_))).WillOnce(testing::Return(QString {})).WillOnce(testing::Return(false)).WillOnce(testing::Return(3));
	EXPECT_CALL(*m_ProgressController, RegisterObserver);
	const auto getTransaction = [] {
		auto transaction = CreateTransaction();
		EXPECT_CALL(*transaction, CreateCommand).WillOnce(testing::Return(CreateCommand())).WillOnce(testing::Return(CreateCommand()));
		EXPECT_CALL(*transaction, Commit);
		return transaction;
	};
	const auto getDatabase = [&] {
		auto database = CreateDatabase<DB::Test::MockDatabaseWritable>();
		EXPECT_CALL(*database, CreateTransaction).WillOnce(testing::Return(getTransaction()));
		return database;
	};
	EXPECT_CALL(*m_DatabaseProvider, GetDatabaseWritable).WillOnce(testing::Return(getDatabase()));
	EXPECT_TRUE(!!logicFactory->CreateBooksExtractor());
}

TEST_F(LogicFactoryTest, CreateInpxGenerator)
{
	const CollectionTest collection;
	EXPECT_CALL(*m_Settings, Get(testing::_, testing::Matcher<const QVariant&>(testing::_))).WillOnce(testing::Return(QString { "utf-8" }));
	EXPECT_CALL(testing::Const(*m_CollectionProvider), GetActiveCollection()).WillOnce(testing::ReturnRef(collection));
	EXPECT_TRUE(!!logicFactory->CreateInpxGenerator());
}

TEST_F(LogicFactoryTest, CreateUpdateChecker)
{
	EXPECT_TRUE(!!logicFactory->CreateUpdateChecker());
}

TEST_F(LogicFactoryTest, CreateCollectionCleaner)
{
	EXPECT_TRUE(!!logicFactory->CreateCollectionCleaner());
}

TEST_F(LogicFactoryTest, CreateOpdsController)
{
	EXPECT_TRUE(!!logicFactory->CreateOpdsController());
}

TEST_F(LogicFactoryTest, CreateFillTemplateConverter)
{
	EXPECT_TRUE(!!logicFactory->CreateFillTemplateConverter(true));
}

struct LogicFactoryCreateFillTemplateConverter
    : LogicFactoryTest
    , testing::WithParamInterface<QString>
{
};

INSTANTIATE_TEST_SUITE_P(LogicFactoryTestCreateFillTemplateConverterTests, LogicFactoryCreateFillTemplateConverter, testing::Values("None", "FileNameOnly", "AllExceptUserPath", "All"));

TEST_P(LogicFactoryCreateFillTemplateConverter, Test)
{
	EXPECT_CALL(*m_Settings, Get(testing::_, testing::Matcher<const QVariant&>(testing::_))).WillOnce(testing::Return(GetParam()));

	const auto converter = logicFactory->CreateFillTemplateConverter();

	const auto getQuery = [&] {
		auto query = CreateQuery();
		EXPECT_CALL(*query, Execute).WillOnce(testing::Return(true));
		EXPECT_CALL(*query, Eof).WillOnce(testing::Return(true));
		EXPECT_CALL(*query, BindLong(testing::Matcher<size_t>(testing::_), testing::_)).Times(testing::AnyNumber());
		return query;
	};
	auto db = CreateDatabase<DB::Test::MockDatabase>();
	EXPECT_CALL(*db, CreateQuery).WillRepeatedly([&]() {
		return getQuery();
	});

	Util::ExtractedBook book {};
	EXPECT_NO_THROW(converter->Fill(*db, "fileName.fb2", book, "folder"));
}

TEST_F(LogicFactoryTest, CreateSettingsStub)
{
	EXPECT_TRUE(!!logicFactory->CreateSettingsStub());
}

TEST_F(LogicFactoryTest, CreateSettingsDecorator)
{
	const auto settingsDecorator = logicFactory->CreateSettingsDecorator(
		m_Settings,
		std::unordered_map<QString, QVariant> {
			{ "hi", "bye" }
    }
	);
	EXPECT_TRUE(!!settingsDecorator);
	EXPECT_EQ(settingsDecorator->Get("hi").toString(), "bye");
	EXPECT_CALL(*m_Settings, Get(testing::_, testing::Matcher<const QVariant&>(testing::_))).WillOnce(testing::Return(QString { "goodbye" }));
	EXPECT_EQ(settingsDecorator->Get("hello").toString(), "goodbye");
}

TEST_F(LogicFactoryTest, CreateZipProgressCallback)
{
	EXPECT_CALL(*m_ProgressController, Add(100));
	EXPECT_TRUE(!!logicFactory->CreateZipProgressCallback(m_ProgressController));
}

TEST_F(LogicFactoryTest, CreateTemporaryDir)
{
	const auto t0 = logicFactory->CreateTemporaryDir();
	const auto t1 = logicFactory->CreateTemporaryDir();
	const auto t2 = logicFactory->CreateTemporaryDir(true);
	const auto t3 = logicFactory->CreateTemporaryDir(true);
	EXPECT_NE(t0->path(), t1->path());
	EXPECT_NE(t0->path(), t2->path());
	EXPECT_NE(t0->path(), t3->path());
	EXPECT_NE(t1->path(), t2->path());
	EXPECT_NE(t1->path(), t3->path());
	EXPECT_EQ(t2->path(), t3->path());

	static constexpr auto FILE = "file";

	std::vector<QString> files {
		t0->filePath(FILE),
		t1->filePath(FILE),
		t2->filePath(FILE),
		t3->filePath(FILE),
		logicFactory->CreateTemporaryDir(t1->path())->filePath(FILE),
		logicFactory->CreateTemporaryDir(t2->path())->filePath(FILE),
	};
	EXPECT_NE(files[0], files[1]);
	EXPECT_NE(files[0], files[2]);
	EXPECT_NE(files[0], files[3]);
	EXPECT_NE(files[0], files[4]);
	EXPECT_NE(files[0], files[5]);
	EXPECT_NE(files[1], files[2]);
	EXPECT_NE(files[1], files[3]);
	EXPECT_EQ(files[1], files[4]);
	EXPECT_NE(files[1], files[5]);
	EXPECT_EQ(files[2], files[3]);
	EXPECT_NE(files[2], files[4]);
	EXPECT_EQ(files[2], files[5]);
	EXPECT_NE(files[3], files[4]);
	EXPECT_EQ(files[3], files[5]);
	EXPECT_NE(files[4], files[5]);

	EXPECT_TRUE(!!logicFactory->CreateTemporaryDir(t2->filePath(FILE)));
}

TEST_F(LogicFactoryTest, GetExtractedBooksInvalidIndex)
{
	testing::StrictMock<MockAbstractItemModel> model;
	EXPECT_CALL(model, index).WillOnce(testing::Return(QModelIndex {}));
	EXPECT_TRUE(logicFactory->GetExtractedBooks(&model, model.index(0, 0, {}), {}).empty());
}

TEST_F(LogicFactoryTest, GetExtractedBooksNoSelection)
{
	testing::StrictMock<MockAbstractItemModel> model;
	EXPECT_CALL(model, index).Times(2).WillRepeatedly([&](const int row, const int column, const QModelIndex&) {
		return model.CreateIndex(row, column);
	});
	EXPECT_CALL(model, setData).Times(2).WillRepeatedly(testing::Return(false));
	const auto getQuery = [&] {
		auto query = CreateQuery();
		EXPECT_CALL(*query, Execute).WillOnce(testing::Return(true));
		EXPECT_CALL(*query, Next);
		EXPECT_CALL(*query, Eof).WillOnce(testing::Return(false)).WillOnce(testing::Return(true));
		EXPECT_CALL(*query, GetRawString(1)).WillOnce(testing::Return("sf"));
		EXPECT_CALL(*query, GetQString(4)).WillOnce(testing::Return("sf"));
		EXPECT_CALL(*query, GetRawString(0)).WillOnce(testing::Return("001.001"));
		EXPECT_CALL(*query, GetRawString(2)).WillOnce(testing::Return("001"));
		EXPECT_CALL(*query, GetInt).Times(3).WillRepeatedly(testing::Return(1));
		return query;
	};
	const auto getDatabase = [&] {
		auto database = CreateDatabase<DB::Test::MockDatabase>();
		EXPECT_CALL(*database, CreateQuery).WillOnce(testing::Return(getQuery()));
		return database;
	};
	EXPECT_CALL(*m_DatabaseProvider, GetDatabase).WillOnce([&] {
		return getDatabase();
	});
	EXPECT_TRUE(logicFactory->GetExtractedBooks(&model, model.index(0, 0, {}), {}).empty());
	EXPECT_TRUE(logicFactory->GetExtractedBooks(&model, model.index(0, 0, {}), {}).empty());
}

TEST_F(LogicFactoryTest, GetExtractedBooks)
{
	testing::StrictMock<MockAbstractItemModel> model;
	EXPECT_CALL(model, index).WillOnce([&](const int row, const int column, const QModelIndex&) {
		return model.CreateIndex(row, column);
	});
	EXPECT_CALL(model, setData).WillOnce([&](const QModelIndex&, const QVariant& value, int) {
		auto request = value.value<SelectedRequest>();
		request.result->push_back(model.CreateIndex(0, 0));
		return true;
	});
	EXPECT_CALL(model, data).WillRepeatedly([](const QModelIndex&, const int role) -> QVariant {
		switch (role)
		{
			case Role::Id:
				return 42;
			case Role::Folder:
				return "Folder";
			case Role::FileName:
				return "FileName.fb2";
			case Role::Size:
				return 666;
			case Role::AuthorFull:
				return "Author Full Name";
			case Role::Series:
				return "Series Name";
			case Role::SeqNumber:
				return 13;
			case Role::Title:
				return "Book Title";
			case Role::Genre:
				return "001.001";
			case Role::LibID:
				return "42";
			case Role::Lang:
				return "ko";
			default:
				break;
		}
		EXPECT_FALSE(true);
		return {};
	});
	const auto getQuery = [&] {
		auto query = CreateQuery();
		EXPECT_CALL(*query, Execute).WillOnce(testing::Return(true));
		EXPECT_CALL(*query, Eof).WillOnce(testing::Return(true));
		return query;
	};
	const auto getDatabase = [&] {
		auto database = CreateDatabase<DB::Test::MockDatabase>();
		EXPECT_CALL(*database, CreateQuery).WillOnce(testing::Return(getQuery()));
		return database;
	};
	EXPECT_CALL(*m_DatabaseProvider, GetDatabase).WillOnce(testing::Return(getDatabase()));
	EXPECT_FALSE(logicFactory->GetExtractedBooks(&model, model.index(0, 0, {}), {}).empty());
}

TEST_F(LogicFactoryTest, GetCurrentExtractedBookEmpty)
{
	EXPECT_CALL(*m_Settings, Get(testing::_, testing::Matcher<const QVariant&>(testing::_))).WillOnce(testing::Return(false)).WillOnce(testing::Return(QString {}));
	EXPECT_CALL(*m_BookInfoProvider, SetBooksViewMode);
	EXPECT_CALL(*m_BookInfoProvider, SetBookRequestCallback);
	EXPECT_CALL(*m_BookInfoProvider, RegisterObserver);
	const auto book = logicFactory->GetCurrentExtractedBook();
	EXPECT_EQ(book.id, 0);
}

TEST_F(LogicFactoryTest, GetCurrentExtractedBook)
{
	EXPECT_CALL(*m_Settings, Get(testing::_, testing::Matcher<const QVariant&>(testing::_))).WillOnce(testing::Return(false)).WillOnce(testing::Return(QString {}));
	EXPECT_CALL(*m_BookInfoProvider, SetBooksViewMode);
	EXPECT_CALL(*m_BookInfoProvider, SetBookRequestCallback);
	EXPECT_CALL(*m_BookInfoProvider, RegisterObserver);
	const auto bookTreeViewcontroller = logicFactory->GetTreeViewController(ItemType::Books);

	testing::StrictMock<MockAbstractItemModel> model;
	EXPECT_CALL(model, setData).WillOnce([&](const QModelIndex&, const QVariant& value, int) {
		auto request = value.value<SelectedRequest>();
		request.result->push_back(model.CreateIndex(0, 0));
		return true;
	});
	EXPECT_CALL(model, data).WillRepeatedly([](const QModelIndex&, const int role) -> QVariant {
		switch (role)
		{
			case Role::Id:
				return 42;
			case Role::Folder:
				return "Folder";
			case Role::FileName:
				return "FileName.fb2";
			case Role::Size:
				return 666;
			case Role::AuthorFull:
				return "Author Full Name";
			case Role::Series:
				return "Series Name";
			case Role::SeqNumber:
				return 13;
			case Role::Title:
				return "Book Title";
			case Role::Genre:
				return {};
			case Role::LibID:
				return "42";
			case Role::Lang:
				return "ko";
			default:
				break;
		}
		EXPECT_FALSE(true);
		return {};
	});

	testing::StrictMock<MockTreeViewControllerObserver> observer;
	EXPECT_CALL(observer, GetModel).WillOnce(testing::Return(&model));
	EXPECT_CALL(observer, GetCurrentIndex).WillOnce(testing::Return(model.CreateIndex(0, 0)));
	bookTreeViewcontroller->RegisterObserver(&observer);

	const auto getQuery = [&] {
		auto query = CreateQuery();
		EXPECT_CALL(*query, Execute).WillOnce(testing::Return(true));
		EXPECT_CALL(*query, Eof).WillOnce(testing::Return(true));
		return query;
	};
	const auto getDatabase = [&] {
		auto database = CreateDatabase<DB::Test::MockDatabase>();
		EXPECT_CALL(*database, CreateQuery).WillOnce(testing::Return(getQuery()));
		return database;
	};
	EXPECT_CALL(*m_DatabaseProvider, GetDatabase).WillOnce(testing::Return(getDatabase()));

	const auto book = logicFactory->GetCurrentExtractedBook();
	EXPECT_EQ(book.id, 42);
}

TEST_F(LogicFactoryTest, FindBook1)
{
	EXPECT_CALL(*m_MenuCustomizer, IsHidden).WillRepeatedly(testing::Return(true));
	EXPECT_CALL(*m_DatabaseController, RegisterObserver);
	EXPECT_CALL(*m_CollectionProvider, ActiveCollectionExists).WillOnce(testing::Return(false));
	EXPECT_TRUE(!!logicFactory->GetTreeViewController(ItemType::Navigation));

	const CollectionTest collection;
	EXPECT_CALL(testing::Const(*m_CollectionProvider), GetActiveCollection()).WillRepeatedly(testing::ReturnRef(collection));

	EXPECT_CALL(*m_Settings, Get(testing::_, testing::Matcher<const QVariant&>(testing::_))).WillOnce(testing::Return(QString("Authors")));
	EXPECT_CALL(*m_Settings, Set).WillOnce([](const QString&, const QVariant& value, const bool) {
		EXPECT_EQ(value, "Languages");
		return true;
	});
	EXPECT_NO_THROW(logicFactory->FindBook(NavigationMode::Languages));
}

TEST_F(LogicFactoryTest, FindBook2)
{
	EXPECT_CALL(*m_MenuCustomizer, IsHidden).WillRepeatedly(testing::Return(true));
	EXPECT_CALL(*m_DatabaseController, RegisterObserver);
	EXPECT_CALL(*m_CollectionProvider, ActiveCollectionExists).WillOnce(testing::Return(false));
	EXPECT_TRUE(!!logicFactory->GetTreeViewController(ItemType::Navigation));

	EXPECT_CALL(*m_Settings, Get(testing::_, testing::Matcher<const QVariant&>(testing::_))).WillOnce(testing::Return(false)).WillOnce(testing::Return(QString {}));
	EXPECT_CALL(*m_BookInfoProvider, SetBooksViewMode);
	EXPECT_CALL(*m_BookInfoProvider, SetBookRequestCallback);
	EXPECT_CALL(*m_BookInfoProvider, RegisterObserver);
	const auto bookTreeViewController = logicFactory->GetTreeViewController(ItemType::Books);
	EXPECT_TRUE(!!bookTreeViewController);

	const CollectionTest collection;
	EXPECT_CALL(testing::Const(*m_CollectionProvider), GetActiveCollection()).WillRepeatedly(testing::ReturnRef(collection));

	EXPECT_CALL(*m_Settings, Get(testing::_, testing::Matcher<const QVariant&>(testing::_))).WillOnce(testing::Return(QString("Series")));

	const QString mySeries = "My Series";
	EXPECT_CALL(*m_NavigationInfoProvider, GetNavigationID).WillOnce(testing::ReturnRef(mySeries));

	testing::StrictMock<MockTreeViewControllerObserver> observer;
	EXPECT_CALL(observer, OnRestoreCurrentIdRequested);
	bookTreeViewController->RegisterObserver(&observer);

	EXPECT_NO_THROW(logicFactory->FindBook(NavigationMode::Series, mySeries));
}

TEST_F(LogicFactoryTest, FindBook3)
{
	EXPECT_CALL(*m_MenuCustomizer, IsHidden).WillRepeatedly(testing::Return(true));
	EXPECT_CALL(*m_DatabaseController, RegisterObserver);
	EXPECT_CALL(*m_CollectionProvider, ActiveCollectionExists).WillOnce(testing::Return(false));
	EXPECT_TRUE(!!logicFactory->GetTreeViewController(ItemType::Navigation));

	const CollectionTest collection;
	EXPECT_CALL(testing::Const(*m_CollectionProvider), GetActiveCollection()).WillRepeatedly(testing::ReturnRef(collection));

	EXPECT_CALL(*m_Settings, Get(testing::_, testing::Matcher<const QVariant&>(testing::_))).WillOnce(testing::Return(QString("Authors")));
	EXPECT_CALL(*m_Settings, Set)
		.WillOnce([](const QString&, const QVariant& value, const bool) {
			EXPECT_EQ(value, 42);
			return true;
		})
		.WillOnce([](const QString&, const QVariant& value, const bool) {
			EXPECT_EQ(value, "Favorites");
			return true;
		})
		.WillOnce([](const QString&, const QVariant& value, const bool) {
			EXPECT_EQ(value, "Groups");
			return true;
		});
	EXPECT_NO_THROW(logicFactory->FindBook(NavigationMode::Groups, "Favorites", 42));
}

} // namespace
