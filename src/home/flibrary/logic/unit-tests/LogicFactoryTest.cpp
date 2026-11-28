#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "database/interface/IDatabase.h"

#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IScriptController.h"

#include "gutil/mock/MockParentWidgetProvider.h"
#include "mock/logic/MockAnnotationController.h"
#include "mock/logic/MockAuthorAnnotationController.h"
#include "mock/logic/MockBookInteractor.h"
#include "mock/logic/MockCollectionProvider.h"
#include "mock/logic/MockDataProvider.h"
#include "mock/logic/MockDatabaseController.h"
#include "mock/logic/MockDatabaseProvider.h"
#include "mock/logic/MockFilterController.h"
#include "mock/logic/MockMenuCustomizer.h"
#include "mock/logic/MockModelProvider.h"
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
LOGIC_FACTORY_TEST_MOCK_ITEM(CollectionProvider)          \
LOGIC_FACTORY_TEST_MOCK_ITEM(MenuCustomizer)              \
LOGIC_FACTORY_TEST_MOCK_ITEM(NavigationInfoProvider)      \
LOGIC_FACTORY_TEST_MOCK_ITEM(BookInfoProvider)            \
LOGIC_FACTORY_TEST_MOCK_ITEM(AnnotationController)        \
LOGIC_FACTORY_TEST_MOCK_ITEM(AuthorAnnotationController)  \
LOGIC_FACTORY_TEST_MOCK_ITEM(FilterController)            \
LOGIC_FACTORY_TEST_MOCK_ITEM(BookInteractor)

struct LogicFactoryTest : testing::Test
{
#define LOGIC_FACTORY_TEST_MOCK_ITEM(NAME) std::shared_ptr<testing::StrictMock<Mock##NAME>> m_##NAME = std::make_shared<testing::StrictMock<Mock##NAME>>();
	LOGIC_FACTORY_TEST_MOCK_ITEMS_X_MACRO
#undef LOGIC_FACTORY_TEST_MOCK_ITEM

	DI di { [this](Hypodermic::ContainerBuilder& builder, Hypodermic::Container& /*container*/) {
#define LOGIC_FACTORY_TEST_MOCK_ITEM(NAME) builder.registerInstance(m_##NAME).as<I##NAME>();
		LOGIC_FACTORY_TEST_MOCK_ITEMS_X_MACRO
#undef LOGIC_FACTORY_TEST_MOCK_ITEM
	} };

	std::shared_ptr<ILogicFactory> logicFactory { di.Resolve<ILogicFactory>() };

	LogicFactoryTest()
	{
		EXPECT_TRUE(!!logicFactory);
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
}

TEST_F(LogicFactoryTest, GetTreeViewControllerBooks)
{
	EXPECT_CALL(*m_Settings, Get(testing::_, testing::Matcher<const QVariant&>(testing::_))).WillOnce(testing::Return(false)).WillOnce(testing::Return(QString {}));
	EXPECT_CALL(*m_BookInfoProvider, SetBooksViewMode);
	EXPECT_CALL(*m_BookInfoProvider, SetBookRequestCallback);
	EXPECT_CALL(*m_BookInfoProvider, RegisterObserver);
	EXPECT_TRUE(!!logicFactory->GetTreeViewController(ItemType::Books));
}

} // namespace
