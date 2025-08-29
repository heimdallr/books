#include "ReactAppRequester.h"

#include <ranges>

#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "fnd/ScopedCall.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/constants/SettingsConstant.h"

#include "logic/data/DataItem.h"
#include "logic/data/Genre.h"
#include "logic/shared/ImageRestore.h"
#include "util/AnnotationControllerObserver.h"
#include "util/Fb2InpxParser.h"
#include "util/FunctorExecutionForwarder.h"

#include "log.h"

using namespace HomeCompa::Opds;

namespace
{

constexpr auto SEARCH = "search";
constexpr auto SELECTED_ITEM_ID = "selectedItemID";
constexpr auto SELECTED_GROUP_ID = "selectedGroupID";

#define AUTHOR_FULL_NAME "a.LastName || coalesce(' ' || nullif(a.FirstName, ''), '') || coalesce(' ' || nullif(a.MiddleName, ''), '')"

constexpr auto MAIN_BOOK_FIELDS = "b.BookID, b.Title, b.BookSize, b.Lang, b.LibRate, nullif(b.SeqNumber, 0) as SeqNumber, b.FileName, b.Ext, b.UpdateDate";
constexpr auto AUTHORS_FIELD = "(select group_concat(" AUTHOR_FULL_NAME R"(, ', ')
	from Authors a 
	join Author_List al on al.AuthorID = a.AuthorID and al.BookID = b.BookID
) as AuthorsNames
)";
constexpr auto GENRES_FIELD = R"(
(select group_concat(g.GenreAlias, ', ')
	from Genres g
	join Genre_List gl on gl.GenreCode = g.GenreCode and gl.BookID = b.BookID
) as Genres
)";

template <typename T>
QJsonObject FromQuery(const HomeCompa::DB::IQuery& query)
{
	QJsonObject object;
	for (size_t i = 0, sz = query.ColumnCount(); i < sz; ++i)
		object.insert(QString::fromStdString(query.ColumnName(i)), query.IsNull(i) ? QJsonValue {} : query.Get<T>(i));
	return object;
}

QString PrepareSearchTerms(const QString& searchTerms)
{
	auto terms = searchTerms.split(QRegularExpression(R"(\s+|\+)"), Qt::SkipEmptyParts);
	std::ranges::transform(terms, terms.begin(), [](const auto& item) { return item + '*'; });
	return terms.join(' ');
}

QString Get(const IReactAppRequester::Parameters& parameters, const QString& key, const QString& defaultValue = {})
{
	const auto it = parameters.find(key);
	return it != parameters.end() ? it->second : defaultValue;
}

} // namespace

struct ReactAppRequester::Impl
{
	std::shared_ptr<const ISettings> settings;
	std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider;
	std::shared_ptr<const Flibrary::IDatabaseController> databaseController;
	std::shared_ptr<const ICoverCache> coverCache;
	std::shared_ptr<Flibrary::IAnnotationController> annotationController;

	Impl(std::shared_ptr<const ISettings> settings,
	     std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider,
	     std::shared_ptr<const Flibrary::IDatabaseController> databaseController,
	     std::shared_ptr<const ICoverCache> coverCache,
	     std::shared_ptr<Flibrary::IAnnotationController> annotationController)
		: settings { std::move(settings) }
		, collectionProvider { std::move(collectionProvider) }
		, databaseController { std::move(databaseController) }
		, coverCache { std::move(coverCache) }
		, annotationController { std::move(annotationController) }
	{
	}

	QJsonObject getConfig(const Parameters&) const
	{
		const auto db = databaseController->GetDatabase(true);

		QJsonObject result;
		{
			const auto query = db->CreateQuery("select count (42) from Books");
			query->Execute();
			assert(!query->Eof());
			result.insert("numberOfBooks", query->Get<long long>(0));
		}

		{
			QJsonArray array;
			const auto genres = Flibrary::Genre::Load(*db);
			const auto process = [&](const Flibrary::Genre& genre, const auto& f) -> void
			{
				for (const auto& child : genre.children)
				{
					array.append(QJsonObject {
						{  "GenreCode",                child.code },
						{ "ParentCode",                genre.code },
						{    "FB2Code",             child.fb2Code },
						{ "GenreAlias",                child.name },
						{  "IsDeleted", child.removed ? "1" : "0" },
					});
					f(child, f);
				}
			};

			process(genres, process);
			result.insert("genres", array);
		}

		{
			static constexpr std::string_view queryText = R"(
SELECT gu.GroupID, gu.Title
          , (SELECT COUNT(42) FROM Groups_List_User_View WHERE Groups_List_User_View.GroupID = gu.GroupID) AS numberOfBooks
          , (SELECT COUNT(42) FROM Books b join Groups_List_User glu on glu.GroupID = gu.GroupID and glu.ObjectID = b.BookID) AS booksInGroup
          , (SELECT COUNT(42) FROM Authors a join Groups_List_User glu on glu.GroupID = gu.GroupID and glu.ObjectID = a.AuthorID) AS authorsInGroup
          , (SELECT group_concat()" AUTHOR_FULL_NAME R"(, ", ") FROM Authors a join Groups_List_User glu on glu.GroupID = gu.GroupID and glu.ObjectID = a.AuthorID) AS authorsListInGroup
          , (SELECT COUNT(42) FROM Series s join Groups_List_User glu on glu.GroupID = gu.GroupID and glu.ObjectID = s.SeriesID) AS seriesInGroup
          , (SELECT group_concat(s.SeriesTitle, ", ") FROM Series s join Groups_List_User glu on glu.GroupID = gu.GroupID and glu.ObjectID = s.SeriesID) AS seriesListInGroup
          , (SELECT COUNT(42) FROM Keywords k join Groups_List_User glu on glu.GroupID = gu.GroupID and glu.ObjectID = k.KeywordID) AS keywordsInGroup
          , (SELECT group_concat(k.KeywordTitle, ", ") FROM Keywords k join Groups_List_User glu on glu.GroupID = gu.GroupID and glu.ObjectID = k.KeywordID) AS keywordsListInGroup
          FROM Groups_User gu
)";
			QJsonArray array;
			const auto query = db->CreateQuery(queryText);
			for (query->Execute(); !query->Eof(); query->Next())
				array.append(FromQuery<const char*>(*query));
			result.insert("groups", array);
		}

		if (auto readTemplate = settings->Get(Flibrary::Constant::Settings::OPDS_READ_URL_TEMPLATE).toString(); !readTemplate.isEmpty())
		{
			const auto host = settings->Get(Flibrary::Constant::Settings::OPDS_HOST_KEY, Flibrary::Constant::Settings::OPDS_HOST_DEFAULT);
			const auto port = settings->Get(Flibrary::Constant::Settings::OPDS_PORT_KEY, Flibrary::Constant::Settings::OPDS_PORT_DEFAULT);
			readTemplate.replace("%HTTP_HOST%", host);
			readTemplate.replace("%HTTP_PORT%", QString::number(port));

			result.insert("linkToExtBookReader", std::move(readTemplate));
		}

		return result;
	}

	QJsonObject getSearchStats(const Parameters& parameters) const
	{
		static constexpr auto searchQueryText = R"(
with Search (Title) as (
    select ?
)
select (select count (42) from Books_Search join Search s on Books_Search match s.Title) as bookTitles
    , (select count (42) from Authors_Search join Search s on Authors_Search match s.Title) as authors
    , (select count (42) from Series_Search join Search s on Series_Search match s.Title) as bookSeries
)";

		static constexpr auto groupsQueryText = R"(
select (select count (42) from Groups_List_User_View l where l.GroupID = g.GroupID) as bookTitles
    , (select count (distinct al.AuthorID) from Author_List al join Groups_List_User_View gl on gl.BookID = al.BookID and gl.GroupID = g.GroupID) as authors
    , (select count (distinct sl.SeriesID) from Series_List sl join Groups_List_User_View gl on gl.BookID = sl.BookID and gl.GroupID = g.GroupID) as bookSeries
from Groups_User g
where g.GroupID = ?
)";
		static constexpr std::tuple<const char*, const char*, bool> queries[] {
			{ SELECTED_GROUP_ID, groupsQueryText, false },
			{            SEARCH, searchQueryText,  true },
		};

		for (const auto& [key, queryText, isSearch] : queries)
		{
			const auto parameter = Get(parameters, key);
			if (parameter.isEmpty())
				continue;

			const auto query = CreateParameterQuery(queryText, parameter, isSearch);
			query->Execute();

			return QJsonObject {
				{ "searchStats", FromQuery<int>(*query) }
			};
		}

		assert(false);
		return {};
	}

	QJsonObject getSearchTitles(const Parameters& parameters) const
	{
		static constexpr auto queryText = "select %1, %2, %3, s.SeriesTitle from Books b %4 left join Series s on s.SeriesID = b.SeriesID";
		static constexpr auto groupJoin = "join Groups_List_User_View gl on gl.BookID = b.BookID and gl.GroupID = ?";
		static constexpr auto searchJoin = "join Books_Search fts on fts.rowid = b.BookID and Books_Search match ?";

		static constexpr std::tuple<const char*, const char*, bool> list[] {
			{ SELECTED_GROUP_ID,  groupJoin, false },
			{            SEARCH, searchJoin,  true },
		};

		return SelectByParameter(list, QString(queryText).arg(MAIN_BOOK_FIELDS).arg(AUTHORS_FIELD).arg(GENRES_FIELD).arg("%1"), parameters, "titlesList");
	}

	QJsonObject getSearchAuthors(const Parameters& parameters) const
	{
		static constexpr auto queryText = "select a.AuthorID, " AUTHOR_FULL_NAME " as Authors, count(42) as Books from Authors a %1 group by a.AuthorID";
		static constexpr auto groupJoin = "join Author_List al on al.AuthorID = a.AuthorID join Groups_List_User_View gl on gl.BookID = al.BookID and gl.GroupID = ?";
		static constexpr auto searchJoin = "join Author_List al on al.AuthorID = a.AuthorID join Authors_Search fts on fts.rowid = a.AuthorID and Authors_Search match ?";

		static constexpr std::tuple<const char*, const char*, bool> list[] {
			{ SELECTED_GROUP_ID,  groupJoin, false },
			{            SEARCH, searchJoin,  true },
		};

		return SelectByParameter(list, queryText, parameters, "authorsList");
	}

	QJsonObject getSearchSeries(const Parameters& parameters) const
	{
		static constexpr auto queryText = "select s.SeriesID, s.SeriesTitle, count(42) as Books from Series s %1 group by s.SeriesID";
		static constexpr auto groupJoin = "join Series_List sl on sl.SeriesID = s.SeriesID join Groups_List_User_View gl on gl.BookID = sl.BookID and gl.GroupID = ?";
		static constexpr auto searchJoin = "join Series_List sl on sl.SeriesID = s.SeriesID join Series_Search fts on fts.rowid = s.SeriesID and Series_Search match ?";

		static constexpr std::tuple<const char*, const char*, bool> list[] {
			{ SELECTED_GROUP_ID,  groupJoin, false },
			{            SEARCH, searchJoin,  true },
		};

		return SelectByParameter(list, queryText, parameters, "seriesList");
	}

	QJsonObject getSearchAuthorBooks(const Parameters& parameters) const
	{
		static constexpr auto queryText = "select %1, %2, s.SeriesTitle from Books b %3 left join Series s on s.SeriesID = b.SeriesID";
		static constexpr std::tuple<const char*, const char*> list[] {
			{ SELECTED_GROUP_ID, "join Groups_List_User_View gl on gl.BookID = b.BookID and gl.GroupID = %1" },
			{  SELECTED_ITEM_ID,          "join Author_List al on al.BookID = b.BookID and al.AuthorID = %1" },
		};

		return GetBookListByIds(list, QString(queryText).arg(MAIN_BOOK_FIELDS).arg(GENRES_FIELD).arg("%1"), parameters, "titlesList");
	}

	QJsonObject getSearchSeriesBooks(const Parameters& parameters) const
	{
		static constexpr auto queryText = "select %1, %2, %3 from Books b %4";
		static constexpr std::tuple<const char*, const char*> list[] {
			{ SELECTED_GROUP_ID, "join Groups_List_User_View gl on gl.BookID = b.BookID and gl.GroupID = %1" },
			{  SELECTED_ITEM_ID,          "join Series_List sl on sl.BookID = b.BookID and sl.SeriesID = %1" },
		};

		return GetBookListByIds(list, QString(queryText).arg(MAIN_BOOK_FIELDS).arg(AUTHORS_FIELD).arg(GENRES_FIELD).arg("%1"), parameters, "titlesList");
	}

	QJsonObject getBookForm(const Parameters& parameters) const
	{
		QJsonObject result;

		const auto bookId = Get(parameters, SELECTED_ITEM_ID);

		QEventLoop eventLoop;
		AnnotationControllerObserver observer(
			[&](const Flibrary::IAnnotationController::IDataProvider& dataProvider)
			{
				ScopedCall eventLoopGuard([&] { eventLoop.exit(); });

				const auto& book = dataProvider.GetBook();

				QJsonArray authors;

				const auto authorsList = [&]
				{
					QStringList values;
					for (size_t i = 0, sz = dataProvider.GetAuthors().GetChildCount(); i < sz; ++i)
					{
						const auto& authorItem = dataProvider.GetAuthors().GetChild(i);

						authors.append(QJsonObject {
							{   "AuthorID",											  authorItem->GetId() },
							{  "FirstName",  authorItem->GetRawData(Flibrary::AuthorItem::Column::FirstName) },
							{   "LastName",   authorItem->GetRawData(Flibrary::AuthorItem::Column::LastName) },
							{ "MiddleName", authorItem->GetRawData(Flibrary::AuthorItem::Column::MiddleName) },
						});

						values << QString("%1 %2 %3")
									  .arg(authorItem->GetRawData(Flibrary::AuthorItem::Column::LastName),
					                       authorItem->GetRawData(Flibrary::AuthorItem::Column::FirstName),
					                       authorItem->GetRawData(Flibrary::AuthorItem::Column::MiddleName))
									  .split(' ', Qt::SkipEmptyParts)
									  .join(' ');
					}
					return values.join(", ");
				}();

				const auto genresList = [&]
				{
					QStringList values;
					for (size_t i = 0, sz = dataProvider.GetGenres().GetChildCount(); i < sz; ++i)
						values << dataProvider.GetGenres().GetChild(i)->GetRawData(Flibrary::NavigationItem::Column::Title);
					return values.join(", ");
				}();

				QJsonArray series;
				const auto& bookSeries = dataProvider.GetSeries();
				for (size_t i = 0, sz = bookSeries.GetChildCount(); i < sz; ++i)
				{
					const auto item = bookSeries.GetChild(i);
					QJsonObject obj {
						{    "SeriesID",										 item->GetId() },
						{ "SeriesTitle", item->GetRawData(Flibrary::SeriesItem::Column::Title) },
					};
					if (const auto seqNum = Util::Fb2InpxParser::GetSeqNumber(item->GetRawData(Flibrary::SeriesItem::Column::SeqNum)); !seqNum.isEmpty())
						obj.insert("SeqNumber", seqNum);
					series.append(obj);
				}

				QJsonObject bookForm {
					{       "BookID",																			  book.GetId() },
					{     "BookSize",										 book.GetRawData(Flibrary::BookItem::Column::Size) },
					{     "FileName",               QFileInfo(book.GetRawData(Flibrary::BookItem::Column::FileName)).baseName() },
					{          "Ext",           "." + QFileInfo(book.GetRawData(Flibrary::BookItem::Column::FileName)).suffix() },
					{         "Lang",										 book.GetRawData(Flibrary::BookItem::Column::Lang) },
					{      "LibRate",									  book.GetRawData(Flibrary::BookItem::Column::LibRate) },
					{    "SeqNumber", Util::Fb2InpxParser::GetSeqNumber(book.GetRawData(Flibrary::BookItem::Column::SeqNumber)) },
					{  "SeriesTitle",									   book.GetRawData(Flibrary::BookItem::Column::Series) },
					{        "Title",										book.GetRawData(Flibrary::BookItem::Column::Title) },
					{ "AuthorsNames",																			   authorsList },
					{       "Genres",																				genresList },
				};

				result.insert("annotation", dataProvider.GetAnnotation());
				result.insert("city", dataProvider.GetPublishCity());
				result.insert("isbn", dataProvider.GetPublishIsbn());
				result.insert("publisher", dataProvider.GetPublisher());
				result.insert("year", dataProvider.GetPublishYear());

				result.insert("authors", authors);
				result.insert("bookForm", bookForm);
				result.insert("series", series);

				if (const auto& covers = dataProvider.GetCovers(); !covers.empty())
					coverCache->Set(bookId, std::move(Flibrary::Recode(covers.front().bytes).first));
			});

		annotationController->RegisterObserver(&observer);
		annotationController->SetCurrentBookId(bookId, true);
		eventLoop.exec();

		return result;
	}

private:
	std::unique_ptr<DB::IQuery> CreateParameterQuery(const QString& queryText, const QString& parameter, const bool isSearch) const
	{
		const auto db = databaseController->GetDatabase(true);
		auto query = db->CreateQuery(queryText.toStdString());
		query->Bind(0, (isSearch ? PrepareSearchTerms(parameter) : parameter).toStdString());
		return query;
	}

	template <std::ranges::range T>
	QJsonObject SelectByParameter(const T& list, const QString& queryText, const Parameters& parameters, const QString& resultName) const
	{
		for (const auto& [key, join, isSearch] : list)
		{
			const auto parameter = Get(parameters, key);
			if (!parameter.isEmpty())
				return GetBookListByParameter(QString(queryText).arg(join), parameter, resultName, isSearch);
		}

		assert(false);
		return {};
	}

	QJsonObject GetBookListByParameter(const QString& queryText, const QString& parameter, const QString& resultName, const bool isSearch) const
	{
		QJsonArray array;

		const auto query = CreateParameterQuery(queryText, parameter, isSearch);
		for (query->Execute(); !query->Eof(); query->Next())
			array.append(FromQuery<const char*>(*query));

		return QJsonObject {
			{ resultName, array }
		};
	}

	template <std::ranges::range T>
	QJsonObject GetBookListByIds(const T& list, const QString& queryText, const Parameters& parameters, const QString& resultName) const
	{
		QJsonArray array;

		QStringList joins;
		std::ranges::transform(list | std::views::filter([&](const auto& item) { return parameters.contains(std::get<0>(item)); }),
		                       std::back_inserter(joins),
		                       [&](const auto& item) { return QString(std::get<1>(item)).arg(parameters.at(std::get<0>(item))); });

		const auto db = databaseController->GetDatabase(true);
		auto query = db->CreateQuery(queryText.arg(joins.join('\n')).toStdString());
		for (query->Execute(); !query->Eof(); query->Next())
			array.append(FromQuery<const char*>(*query));

		return QJsonObject {
			{ resultName, array }
		};
	}

private:
	Util::FunctorExecutionForwarder m_forwarder;
};

namespace
{
template <typename Obj, typename NavigationGetter, typename... ARGS>
QByteArray GetImpl(Obj& obj, NavigationGetter getter, const ARGS&... args)
{
	if (!obj.collectionProvider->ActiveCollectionExists())
		return {};

	QByteArray bytes;
	try
	{
		const QJsonDocument doc(std::invoke(getter, std::cref(obj), std::cref(args)...));
		bytes = doc.toJson();
	}
	catch (const std::exception& ex)
	{
		PLOGE << ex.what();
		return {};
	}
	catch (...)
	{
		PLOGE << "Unknown error";
		return {};
	}

#ifndef NDEBUG
	PLOGV << bytes;
#endif

	return bytes;
}

} // namespace

ReactAppRequester::ReactAppRequester(std::shared_ptr<const ISettings> settings,
                                     std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider,
                                     std::shared_ptr<const Flibrary::IDatabaseController> databaseController,
                                     std::shared_ptr<const ICoverCache> coverCache,
                                     std::shared_ptr<Flibrary::IAnnotationController> annotationController)
	: m_impl(std::move(settings), std::move(collectionProvider), std::move(databaseController), std::move(coverCache), std::move(annotationController))
{
}

ReactAppRequester::~ReactAppRequester() = default;

#define OPDS_GET_BOOKS_API_ITEM(NAME)                                      \
	QByteArray ReactAppRequester::NAME(const Parameters& parameters) const \
	{                                                                      \
		return GetImpl(*m_impl, &Impl::NAME, parameters);                  \
	}
OPDS_GET_BOOKS_API_ITEMS_X_MACRO
#undef OPDS_GET_BOOKS_API_ITEM
