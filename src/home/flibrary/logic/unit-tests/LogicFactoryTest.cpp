#include <gtest/gtest.h>

#include "database/interface/IDatabase.h"

#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IScriptController.h"

#include "unit-test-util/book.h"
#include "unit-test-util/database.h"

using namespace HomeCompa::DB;
using namespace HomeCompa::Flibrary;
using namespace HomeCompa::Test;
using namespace HomeCompa::Util;

namespace {

namespace FillScriptTemplateTest {

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

} // namespace FillScriptTemplateTest

TEST(LogicFactoryTest, Stub)
{
	SUCCEED();
}

TEST(LogicFactoryTest, FillScriptTemplate)
{
	const auto db    = CreateDatabase<IDatabase>();
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

} // namespace
