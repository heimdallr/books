#include <ranges>

#include <QFileInfo>
#include <QLineEdit>
#include <QMenu>
#include <QUuid>

#include "fnd/FindPair.h"
#include "fnd/ScopedCall.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "constants/Localization.h"
#include "constants/ModelRole.h"
#include "constants/ProductConstant.h"
#include "logic/IAnnotationController.h"
#include "logic/IDatabaseUser.h"
#include "logic/IFilterProvider.h"
#include "logic/ILogicFactory.h"
#include "logic/IScriptController.h"
#include "ui/IStyleApplier.h"
#include "util/files.h"
#include "util/localization.h"

namespace HomeCompa::Flibrary
{

namespace
{

void SetMacroImpl(QString& str, const IScriptController::Macro macro, const QString& value)
{
	const QString macroStr = IScriptController::GetMacro(macro);
	const auto start = str.indexOf(macroStr, 0, Qt::CaseInsensitive);
	if (start < 0)
		return;

	const auto replace = [&](const QString& s, const qsizetype startPos, const qsizetype endPos)
	{
		str.erase(std::next(str.begin(), startPos), std::next(str.begin(), endPos));
		str.insert(startPos, s);
	};

	if (start == 0 || str[start - 1] != '[')
		return replace(value, start, start + macroStr.length());

	const auto itEnd = std::find_if(std::next(str.cbegin(), start),
	                                str.cend(),
	                                [n = 1](const QChar ch) mutable
	                                {
										if (ch == '[')
											++n;

										return ch == ']' && --n == 0;
									});

	if (itEnd == str.cend())
		return replace(value, start, start + macroStr.length());

	if (value.isEmpty())
		return replace(value, start - 1, std::distance(str.cbegin(), itEnd) + 1);

	str.erase(itEnd);
	str.erase(std::next(str.begin(), start) - 1);
	return replace(value, start - 1, start + macroStr.length() - 1);
}

QString ApplyMacroSourceFile(DB::IDatabase&, const ILogicFactory::ExtractedBook&, const QFileInfo&, const QStringList&)
{
	return {};
}

QString ApplyMacroUserDestinationFolder(DB::IDatabase&, const ILogicFactory::ExtractedBook&, const QFileInfo&, const QStringList&)
{
	return {};
}

QString ApplyMacroTitle(DB::IDatabase&, const ILogicFactory::ExtractedBook& book, const QFileInfo&, const QStringList&)
{
	return Util::RemoveIllegalPathCharacters(book.title);
}

QString ApplyMacroFileName(DB::IDatabase&, const ILogicFactory::ExtractedBook&, const QFileInfo& fileInfo, const QStringList&)
{
	return Util::RemoveIllegalPathCharacters(fileInfo.fileName());
}

QString ApplyMacroFileExt(DB::IDatabase&, const ILogicFactory::ExtractedBook&, const QFileInfo& fileInfo, const QStringList&)
{
	return Util::RemoveIllegalPathCharacters(fileInfo.suffix());
}

QString ApplyMacroBaseFileName(DB::IDatabase&, const ILogicFactory::ExtractedBook&, const QFileInfo& fileInfo, const QStringList&)
{
	return Util::RemoveIllegalPathCharacters(fileInfo.completeBaseName());
}

QString ApplyMacroAuthor(DB::IDatabase&, const ILogicFactory::ExtractedBook& book, const QFileInfo&, const QStringList&)
{
	return Util::RemoveIllegalPathCharacters(book.author);
}

QString ApplyMacroAuthorLastFM(DB::IDatabase&, const ILogicFactory::ExtractedBook&, const QFileInfo&, const QStringList& authorNameSplitted)
{
	return QString("%1 %2%3")
	    .arg(!authorNameSplitted.empty() ? authorNameSplitted[0] : QString {})
	    .arg(authorNameSplitted.size() > 1 ? QString("%1.").arg(authorNameSplitted[1][0]) : QString {})
	    .arg(authorNameSplitted.size() > 2 ? QString("%1.").arg(authorNameSplitted[2][0]) : QString {})
	    .simplified();
}

QString ApplyMacroAuthorLastName(DB::IDatabase&, const ILogicFactory::ExtractedBook&, const QFileInfo&, const QStringList& authorNameSplitted)
{
	return !authorNameSplitted.empty() ? authorNameSplitted[0] : QString {};
}

QString ApplyMacroAuthorFirstName(DB::IDatabase&, const ILogicFactory::ExtractedBook&, const QFileInfo&, const QStringList& authorNameSplitted)
{
	return authorNameSplitted.size() > 1 ? authorNameSplitted[1] : QString {};
}

QString ApplyMacroAuthorMiddleName(DB::IDatabase&, const ILogicFactory::ExtractedBook&, const QFileInfo&, const QStringList& authorNameSplitted)
{
	return authorNameSplitted.size() > 2 ? authorNameSplitted[2] : QString {};
}

QString ApplyMacroAuthorF(DB::IDatabase&, const ILogicFactory::ExtractedBook&, const QFileInfo&, const QStringList& authorNameSplitted)
{
	return authorNameSplitted.size() > 1 ? authorNameSplitted[1][0] : QString {};
}

QString ApplyMacroAuthorM(DB::IDatabase&, const ILogicFactory::ExtractedBook&, const QFileInfo&, const QStringList& authorNameSplitted)
{
	return authorNameSplitted.size() > 2 ? authorNameSplitted[2][0] : QString {};
}

QString ApplyMacroSeries(DB::IDatabase&, const ILogicFactory::ExtractedBook& book, const QFileInfo&, const QStringList&)
{
	return Util::RemoveIllegalPathCharacters(book.series);
}

QString ApplyMacroSeqNumber(DB::IDatabase&, const ILogicFactory::ExtractedBook& book, const QFileInfo&, const QStringList&)
{
	return book.seqNumber > 0 ? QString::number(book.seqNumber) : QString {};
}

QString ApplyMacroFileSize(DB::IDatabase&, const ILogicFactory::ExtractedBook& book, const QFileInfo&, const QStringList&)
{
	return QString::number(book.size);
}

QString ApplyMacroGenre(DB::IDatabase&, const ILogicFactory::ExtractedBook& book, const QFileInfo&, const QStringList&)
{
	return Util::RemoveIllegalPathCharacters(book.genre);
}

QString ApplyMacroGenreTree(DB::IDatabase&, const ILogicFactory::ExtractedBook& book, const QFileInfo&, const QStringList&)
{
	QStringList genreTree;
	std::ranges::transform(book.genreTree, std::back_inserter(genreTree), &Util::RemoveIllegalPathCharacters);
	return genreTree.join('/');
}

QString ApplyMacroLanguage(DB::IDatabase&, const ILogicFactory::ExtractedBook& book, const QFileInfo&, const QStringList&)
{
	return book.lang;
}

QString ApplyQuery(DB::IDatabase& db, const ILogicFactory::ExtractedBook& book, const std::string_view queryText)
{
	const auto query = db.CreateQuery(queryText);
	query->Bind(0, book.id);
	query->Execute();
	return query->Eof() ? QString {} : Util::RemoveIllegalPathCharacters(query->Get<const char*>(0));
}

QString ApplyMacroKeyword(DB::IDatabase& db, const ILogicFactory::ExtractedBook& book, const QFileInfo&, const QStringList&)
{
	return ApplyQuery(db, book, "select k.KeywordTitle from Keywords k join Keyword_List l on l.KeywordID = k.KeywordID and l.BookID = ? order by l.OrdNum limit 1");
}

QString ApplyMacroAllKeywords(DB::IDatabase& db, const ILogicFactory::ExtractedBook& book, const QFileInfo&, const QStringList&)
{
	return ApplyQuery(db, book, "select group_concat(k.KeywordTitle, '_') from Keywords k join Keyword_List l on l.KeywordID = k.KeywordID and l.BookID = ? order by l.OrdNum");
}

QString ApplyMacroId(DB::IDatabase&, const ILogicFactory::ExtractedBook& book, const QFileInfo&, const QStringList&)
{
	return QString::number(book.id);
}

QString ApplyMacroLibId(DB::IDatabase&, const ILogicFactory::ExtractedBook& book, const QFileInfo&, const QStringList&)
{
	return QString::number(book.libId);
}

QString ApplyMacroUid(DB::IDatabase&, const ILogicFactory::ExtractedBook&, const QFileInfo&, const QStringList&)
{
	return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

constexpr std::pair<IScriptController::Macro, QString (*)(DB::IDatabase& db, const ILogicFactory::ExtractedBook&, const QFileInfo&, const QStringList&)> MACRO_APPLIERS[] {
#define SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(NAME) { IScriptController::Macro::NAME, &ApplyMacro##NAME },
	SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEMS_X_MACRO
#undef SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM
};

} // namespace

static_assert(static_cast<size_t>(IScriptController::Macro::Last) == std::size(IScriptController::s_commandMacros));
#define SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM(NAME) static_assert(IScriptController::Macro::NAME == IScriptController::s_commandMacros[static_cast<int>(IScriptController::Macro::NAME)].first);
SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEMS_X_MACRO
#undef SCRIPT_CONTROLLER_TEMPLATE_MACRO_ITEM

bool IScriptController::HasMacro(const QString& str, const Macro macro)
{
	return str.contains(GetMacro(macro));
}

QString& IScriptController::SetMacro(QString& str, const Macro macro, const QString& value)
{
	while (true)
	{
		QString tmp = str;
		SetMacroImpl(str, macro, value);
		if (tmp == str)
			return str;
	}
}

const char* IScriptController::GetMacro(const Macro macro)
{
	return s_commandMacros[static_cast<int>(macro)].second;
}

void IScriptController::ExecuteContextMenu(QLineEdit* lineEdit)
{
	QMenu menu(lineEdit);
	for (const auto& item : s_commandMacros | std::views::values)
	{
		const auto menuItemTitle = QString("%1\t%2").arg(Loc::Tr(s_context, item), item);
		menu.addAction(menuItemTitle,
		               [=, value = QString(item)]
		               {
						   auto currentText = lineEdit->text();
						   const auto currentPosition = lineEdit->cursorPosition();
						   lineEdit->setText(currentText.insert(currentPosition, value));
						   lineEdit->setCursorPosition(currentPosition + static_cast<int>(value.size()));
					   });
	}
	menu.setFont(lineEdit->font());
	menu.exec(QCursor::pos());
}

QString IScriptController::GetDefaultOutputFileNameTemplate()
{
	return QString("%1/%2/[%3/[%4-]]%5.%6")
	    .arg(GetMacro(Macro::UserDestinationFolder))
	    .arg(GetMacro(Macro::Author))
	    .arg(GetMacro(Macro::Series))
	    .arg(GetMacro(Macro::SeqNumber))
	    .arg(GetMacro(Macro::Title))
	    .arg(GetMacro(Macro::FileExt));
}

std::vector<std::vector<QString>> ILogicFactory::GetSelectedBookIds(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, const std::vector<int>& roles)
{
	QModelIndexList selected;
	model->setData({}, QVariant::fromValue(SelectedRequest { index, indexList, &selected }), Role::Selected);

	std::vector<std::vector<QString>> result;
	result.reserve(selected.size());
	std::ranges::transform(selected,
	                       std::back_inserter(result),
	                       [&](const auto& selectedIndex)
	                       {
							   std::vector<QString> resultItem;
							   std::ranges::transform(roles, std::back_inserter(resultItem), [&](const int role) { return selectedIndex.data(role).toString(); });
							   return resultItem;
						   });

	return result;
}

void ILogicFactory::FillScriptTemplate(DB::IDatabase& db, QString& scriptTemplate, const ExtractedBook& book)
{
	const auto authorNameSplitted = Util::RemoveIllegalPathCharacters(book.author).split(' ', Qt::SkipEmptyParts);
	const QFileInfo fileInfo(book.file);
	for (const auto [macro, applier] : MACRO_APPLIERS)
	{
		const auto value = std::invoke(applier, std::ref(db), std::cref(book), std::cref(fileInfo), std::cref(authorNameSplitted));
		IScriptController::SetMacro(scriptTemplate, macro, value);
	}
}

QString IDatabaseUser::GetDatabaseVersionStatement()
{
	return QString("insert or replace into Settings(SettingID, SettingValue) values(%1, %2)").arg(static_cast<int>(Key::DatabaseVersion)).arg(Constant::FlibraryDatabaseVersionNumber);
}

namespace
{
constexpr std::pair<IStyleApplier::Type, const char*> TYPES[] {
#define STYLE_APPLIER_TYPE_ITEM(NAME) { IStyleApplier::Type::NAME, #NAME },
	STYLE_APPLIER_TYPE_ITEMS_X_MACRO
#undef STYLE_APPLIER_TYPE_ITEM
};

#define STYLE_APPLIER_TYPE_ITEM(NAME) static_assert(IStyleApplier::Type::NAME == TYPES[static_cast<size_t>(IStyleApplier::Type::NAME)].first);
STYLE_APPLIER_TYPE_ITEMS_X_MACRO
#undef STYLE_APPLIER_TYPE_ITEM
}

IStyleApplier::Type IStyleApplier::TypeFromString(const char* name)
{
	return FindFirst(TYPES, name, PszComparer {});
}

QString IStyleApplier::TypeToString(const Type type)
{
	return TYPES[static_cast<size_t>(type)].second;
}

namespace
{
constexpr auto ANNOTATION_CONTEXT = "Annotation";
}

void IAnnotationController::IStrategy::AddImpl(Section, QString& text, const QString& str, const char* pattern)
{
	if (!str.isEmpty())
		text.append(QString("<p>%1</p>").arg(Loc::Tr(ANNOTATION_CONTEXT, pattern).arg(str)));
}

QString IAnnotationController::IStrategy::AddTableRowImpl(const char* name, const QString& value)
{
	return QString(R"(<tr><td style="vertical-align: top; padding-right: 7px;">%1</td><td>%2</td></tr>)").arg(Loc::Tr(ANNOTATION_CONTEXT, name)).arg(value);
}

QString IAnnotationController::IStrategy::AddTableRowImpl(const QStringList& values)
{
	QString str;
	ScopedCall tr([&] { str.append("<tr>"); }, [&] { str.append("</tr>"); });
	for (const auto& value : values)
	{
		ScopedCall td([&] { str.append(R"(<td style="vertical-align: top; padding-right: 7px;">)"); }, [&] { str.append("</td>"); });
		str.append(value);
	}
	return str;
}

QString IAnnotationController::IStrategy::TableRowsToStringImpl(const QStringList& values)
{
	return values.isEmpty() ? QString {} : QString("<table>%1</table>\n").arg(values.join("\n"));
}

namespace
{
constexpr auto AUTHORS = "Authors";
constexpr auto AUTHOR_ID = "AuthorID";
constexpr auto SERIES = "Series";
constexpr auto SERIES_ID = "SeriesID";
constexpr auto GENRES = "Genres";
constexpr auto GENRE_CODE = "GenreCode";
constexpr auto KEYWORDS = "Keywords";
constexpr auto KEYWORD_ID = "KeywordID";
constexpr auto LANGUAGES = "Languages";
constexpr auto LANGUAGE_ID = "LanguageCode";

template <typename StatementType, typename>
struct Binder
{
	static void Bind(StatementType& s, size_t index, const QString& value) = delete;
};

template <typename StatementType>
struct Binder<StatementType, int>
{
	static void Bind(StatementType& s, size_t index, const QString& value)
	{
		s.Bind(index, value.toInt());
	}
};

template <typename StatementType>
struct Binder<StatementType, std::string>
{
	static void Bind(StatementType& s, size_t index, const QString& value)
	{
		s.Bind(index, value.toStdString());
	}
};

constexpr IFilterProvider::FilteredNavigation FILTERED_NAVIGATION_DESCRIPTION[] {
	// clang-format off
		{ NavigationMode::Authors    , Loc::Authors     , &IModelProvider::CreateFilterListModel, AUTHORS  , AUTHOR_ID  , &Binder<DB::ICommand, int        >::Bind, &Binder<DB::IQuery, int        >::Bind },
		{ NavigationMode::Series     , Loc::Series      , &IModelProvider::CreateFilterListModel, SERIES   , SERIES_ID  , &Binder<DB::ICommand, int        >::Bind, &Binder<DB::IQuery, int        >::Bind },
		{ NavigationMode::Genres     , Loc::Genres      , &IModelProvider::CreateFilterTreeModel, GENRES   , GENRE_CODE , &Binder<DB::ICommand, std::string>::Bind, &Binder<DB::IQuery, std::string>::Bind },
		{ NavigationMode::PublishYear, Loc::PublishYears, &IModelProvider::CreateFilterTreeModel },
		{ NavigationMode::Keywords   , Loc::Keywords    , &IModelProvider::CreateFilterListModel, KEYWORDS , KEYWORD_ID , &Binder<DB::ICommand, int        >::Bind, &Binder<DB::IQuery, int        >::Bind },
		{ NavigationMode::Updates    , Loc::Updates     , &IModelProvider::CreateFilterTreeModel },
		{ NavigationMode::Archives   , Loc::Archives    , &IModelProvider::CreateFilterListModel },
		{ NavigationMode::Languages  , Loc::Languages   , &IModelProvider::CreateFilterListModel, LANGUAGES, LANGUAGE_ID, &Binder<DB::ICommand, std::string>::Bind, &Binder<DB::IQuery, std::string>::Bind },
		{ NavigationMode::Groups     , Loc::Groups      , &IModelProvider::CreateFilterListModel },
		{ NavigationMode::Search     , Loc::Search      , &IModelProvider::CreateFilterListModel },
		{ NavigationMode::Reviews    , Loc::Reviews     , &IModelProvider::CreateFilterListModel },
		{ NavigationMode::AllBooks   , Loc::AllBooks    , &IModelProvider::CreateFilterListModel },
	// clang-format on
};

static_assert(static_cast<size_t>(NavigationMode::Last) == std::size(FILTERED_NAVIGATION_DESCRIPTION));
#define NAVIGATION_MODE_ITEM(NAME) static_assert(NavigationMode::NAME == FILTERED_NAVIGATION_DESCRIPTION[static_cast<size_t>(NavigationMode::NAME)].navigationMode);
NAVIGATION_MODE_ITEMS_X_MACRO
#undef NAVIGATION_MODE_ITEM

} // namespace

const IFilterProvider::FilteredNavigation& IFilterProvider::GetFilteredNavigationDescription(const NavigationMode navigationMode)
{
	const auto index = static_cast<size_t>(navigationMode);
	assert(index < std::size(FILTERED_NAVIGATION_DESCRIPTION));
	return FILTERED_NAVIGATION_DESCRIPTION[index];
}

} // namespace HomeCompa::Flibrary
