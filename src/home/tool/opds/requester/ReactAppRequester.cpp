#include "ReactAppRequester.h"

#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "fnd/ScopedCall.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "logic/data/DataItem.h"
#include "logic/data/Genre.h"
#include "util/AnnotationControllerObserver.h"
#include "util/Fb2InpxParser.h"
#include "util/FunctorExecutionForwarder.h"

#include "log.h"

using namespace HomeCompa::Opds;

namespace
{

template <typename T>
QJsonObject FromQuery(const HomeCompa::DB::IQuery& query)
{
	QJsonObject object;
	for (size_t i = 0, sz = query.ColumnCount(); i < sz; ++i)
		object.insert(QString::fromStdString(query.ColumnName(i)), query.Get<T>(i));
	return object;
}

QString PrepareSearchTerms(const QString& searchTerms)
{
	auto terms = searchTerms.split(QRegularExpression(R"(\s+|\+)"), Qt::SkipEmptyParts);
	std::ranges::transform(terms, terms.begin(), [](const auto& item) { return item + '*'; });
	return terms.join(' ');
}

}

struct ReactAppRequester::Impl
{
	std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider;
	std::shared_ptr<const Flibrary::IDatabaseController> databaseController;
	std::shared_ptr<const ICoverCache> coverCache;
	std::shared_ptr<Flibrary::IAnnotationController> annotationController;

	Impl(std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider,
	     std::shared_ptr<const Flibrary::IDatabaseController> databaseController,
	     std::shared_ptr<const ICoverCache> coverCache,
	     std::shared_ptr<Flibrary::IAnnotationController> annotationController)
		: collectionProvider { std::move(collectionProvider) }
		, databaseController { std::move(databaseController) }
		, coverCache { std::move(coverCache) }
		, annotationController { std::move(annotationController) }
	{
	}

	QJsonObject getConfig(const QString&) const
	{
		const auto db = databaseController->GetDatabase(true);

		QJsonObject result;
		{
			auto query = db->CreateQuery("select count (42) from Books");
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

		return result;
	}

	QJsonObject getSearchStats(const QString& searchTerms) const
	{
		static constexpr auto queryText = R"(
with Search (Title) as (
    select ?
)
select (select count (42) from Books_Search join Search s on Books_Search match s.Title) as bookTitles
    , (select count (42) from Authors_Search join Search s on Authors_Search match s.Title) as authors
    , (select count (42) from Series_Search join Search s on Series_Search match s.Title) as bookSeries
)";
		const auto query = CreateSearchQuery(queryText, searchTerms);
		query->Execute();
		assert(!query->Eof());

		return QJsonObject {
			{ "searchStats", FromQuery<int>(*query) }
		};
	}

	QJsonObject getSearchTitles(const QString& searchTerms) const
	{
		static constexpr auto queryText = R"(
select b.BookID, b.Title, b.BookSize, b.Lang, b.LibRate, s.SeriesTitle, nullif(b.SeqNumber, 0) as SeqNumber, b.FileName, (
    select group_concat(a.LastName || coalesce(' ' || nullif(a.FirstName, ''), '') || coalesce(' ' || nullif(a.MiddleName, ''), ''), ', ')
        from Authors a 
        join Author_List al on al.AuthorID = a.AuthorID and al.BookID = b.BookID
    ) as AuthorsNames, (
    select group_concat(g.GenreAlias, ', ')
        from Genres g
        join Genre_List gl on gl.GenreCode = g.GenreCode and gl.BookID = b.BookID
    ) as Genres
from Books b
join Books_Search fts on fts.rowid = b.BookID and Books_Search match ?
left join Series s on s.SeriesID = b.SeriesID
)";
		return GetBookListBySearch(queryText, searchTerms, "titlesList");
	}

	QJsonObject getSearchAuthors(const QString& searchTerms) const
	{
		static constexpr auto queryText = R"(
select a.AuthorID, a.LastName || coalesce(' ' || nullif(a.FirstName, ''), '') || coalesce(' ' || nullif(a.MiddleName, ''), '') as Authors, count(42) as Books
from Authors a
join Authors_Search fts on fts.rowid = a.AuthorID and Authors_Search match ?
join Author_List al on al.AuthorID = a.AuthorID
group by a.AuthorID
)";
		return GetBookListBySearch(queryText, searchTerms, "authorsList");
	}

	QJsonObject getSearchSeries(const QString& searchTerms) const
	{
		static constexpr auto queryText = R"(
select s.SeriesID, s.SeriesTitle, count(42) as Books
from Series s
join Series_Search fts on fts.rowid = s.SeriesID and Series_Search match ?
join Series_List sl on sl.SeriesID = s.SeriesID
group by s.SeriesID
)";
		return GetBookListBySearch(queryText, searchTerms, "seriesList");
	}

	QJsonObject getSearchAuthorBooks(const QString& authorId) const
	{
		static constexpr auto queryText = R"(
select b.BookID, b.Title, b.BookSize, b.Lang, b.LibRate, s.SeriesTitle, nullif(b.SeqNumber, 0) as SeqNumber, (
    select group_concat(g.GenreAlias, ', ')
        from Genres g
        join Genre_List gl on gl.GenreCode = g.GenreCode and gl.BookID = b.BookID
    ) as Genres
from Books b
join Author_List al on al.BookID = b.BookID and al.AuthorID = ?
left join Series s on s.SeriesID = b.SeriesID
)";
		return GetBookListById(queryText, authorId, "titlesList");
	}

	QJsonObject getSearchSeriesBooks(const QString& seriesId) const
	{
		static constexpr auto queryText = R"(
select b.BookID, b.Title, b.BookSize, b.Lang, b.LibRate, nullif(b.SeqNumber, 0) as SeqNumber, (
    select group_concat(a.LastName || coalesce(' ' || nullif(a.FirstName, ''), '') || coalesce(' ' || nullif(a.MiddleName, ''), ''), ', ')
        from Authors a 
        join Author_List al on al.AuthorID = a.AuthorID and al.BookID = b.BookID
    ) as AuthorsNames, (
    select group_concat(g.GenreAlias, ', ')
        from Genres g
        join Genre_List gl on gl.GenreCode = g.GenreCode and gl.BookID = b.BookID
    ) as Genres
from Books b
join Series_List sl on sl.BookID = b.BookID and sl.SeriesID = ?
)";
		return GetBookListById(queryText, seriesId, "titlesList");
	}

	QJsonObject getBookForm(const QString& bookId) const
	{
		QJsonObject result;

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
					coverCache->Set(bookId, covers.front().bytes);
			});

		annotationController->RegisterObserver(&observer);
		annotationController->SetCurrentBookId(bookId, true);
		eventLoop.exec();

		return result;
	}

private:
	std::unique_ptr<DB::IQuery> CreateIdQuery(const QString& queryText, const QString& id) const
	{
		const auto db = databaseController->GetDatabase(true);
		auto query = db->CreateQuery(queryText.toStdString());
		bool ok = false;
		query->Bind(0, id.toLongLong(&ok));
		assert(ok);
		return query;
	}

	std::unique_ptr<DB::IQuery> CreateSearchQuery(const QString& queryText, const QString& searchTerms) const
	{
		const auto db = databaseController->GetDatabase(true);
		auto query = db->CreateQuery(queryText.toStdString());
		query->Bind(0, PrepareSearchTerms(searchTerms).toStdString());
		return query;
	}

	QJsonObject GetBookListBySearch(const QString& queryText, const QString& searchTerms, const QString& resultName) const
	{
		QJsonArray array;

		const auto query = CreateSearchQuery(queryText, searchTerms);
		for (query->Execute(); !query->Eof(); query->Next())
			array.append(FromQuery<const char*>(*query));

		return QJsonObject {
			{ resultName, array }
		};
	}

	QJsonObject GetBookListById(const QString& queryText, const QString& id, const QString& resultName) const
	{
		QJsonArray array;

		auto query = CreateIdQuery(queryText, id);
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

ReactAppRequester::ReactAppRequester(std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider,
                                     std::shared_ptr<const Flibrary::IDatabaseController> databaseController,
                                     std::shared_ptr<const ICoverCache> coverCache,
                                     std::shared_ptr<Flibrary::IAnnotationController> annotationController)
	: m_impl(std::move(collectionProvider), std::move(databaseController), std::move(coverCache), std::move(annotationController))
{
}

ReactAppRequester::~ReactAppRequester() = default;

#define OPDS_GET_BOOKS_API_ITEM(NAME, _)                           \
	QByteArray ReactAppRequester::NAME(const QString& value) const \
	{                                                              \
		return GetImpl(*m_impl, &Impl::NAME, value);               \
	}
OPDS_GET_BOOKS_API_ITEMS_X_MACRO
#undef OPDS_GET_BOOKS_API_ITEM
