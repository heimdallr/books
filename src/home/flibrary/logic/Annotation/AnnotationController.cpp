#include "AnnotationController.h"

#include <plog/Log.h>

#include "database/interface/IQuery.h"
#include "shared/DatabaseUser.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

using Extractor = DataItem::Ptr(*)(const DB::IQuery & query, const int * index);
constexpr int QUERY_INDEX_SIMPLE_LIST_ITEM[] { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

constexpr auto BOOK_QUERY = "select %1 from Books b left join Books_User bu on bu.BookID = b.BookID where b.BookID = :id";
constexpr auto SERIES_QUERY = "select s.SeriesID, s.SeriesTitle from Series s join Books b on b.SeriesID = s.seriesID and b.BookID = :id";
constexpr auto AUTHORS_QUERY = "select a.AuthorID, a.LastName, a.FirstName, a.MiddleName from Authors a  join Author_List al on al.AuthorID = a.AuthorID and al.BookID = :id";
constexpr auto GENRES_QUERY = "select g.GenreCode, g.GenreAlias from Genres g join Genre_List gl on gl.GenreCode = g.GenreCode and gl.BookID = :id";
constexpr auto GROUPS_QUERY = "select g.GroupID, g.Title from Groups_User g join Groups_List_User gl on gl.GroupID = g.GroupID and gl.BookID = :id";
}

class AnnotationController::Impl final
{
public:
	explicit Impl(const std::shared_ptr<ILogicFactory> & logicFactory
		, std::shared_ptr<DatabaseUser> databaseUser
	)
		: m_executor(logicFactory->GetExecutor())
		, m_databaseUser(std::move(databaseUser))
	{
	}

public:
	void SetCurrentBookId(QString bookId)
	{
		m_currentBookId = std::move(bookId);
		m_extractInfoTimer->start();
	}

private:
	void ExtractInfo() const
	{
		m_databaseUser->Execute({ "Get book info", [&, id = m_currentBookId.toLongLong()]
		{
			const auto db = m_databaseUser->Database();
			return [&, book = CreateBook(*db, id)] (size_t) mutable
			{
				if (book->GetId() == m_currentBookId)
					ExtractInfo(std::move(book));
			};
		} }, 3);
	}

	void ExtractInfo(DataItem::Ptr book) const
	{
		m_databaseUser->Execute({ "Get book info", [&, book] () mutable
		{
			const auto db = m_databaseUser->Database();
			const auto bookId = book->GetId().toLongLong();
			auto series = CreateSeries(*db, bookId);
			auto authors = CreateDictionary(*db, AUTHORS_QUERY, bookId, &DatabaseUser::CreateFullAuthorItem);
			auto genres = CreateDictionary(*db, GENRES_QUERY, bookId, &DatabaseUser::CreateSimpleListItem);
			auto groups = CreateDictionary(*db, GROUPS_QUERY, bookId, &DatabaseUser::CreateSimpleListItem);
			return [&, book = std::move(book), series = std::move(series), authors = std::move(authors), genres = std::move(genres)] (size_t) mutable
			{
//				if (book->GetId() == m_currentBookId)
//					ExtractInfo(std::move(book));
				};
			} }, 3);
	}

	static DataItem::Ptr CreateBook(DB::IDatabase & db, const long long id)
	{
		const auto query = db.CreateQuery(QString(BOOK_QUERY).arg(DatabaseUser::BOOKS_QUERY_FIELDS).toStdString());
		query->Bind(":id", id);
		query->Execute();
		assert(!query->Eof());
		return DatabaseUser::CreateBookItem(*query);
	}

	static DataItem::Ptr CreateSeries(DB::IDatabase & db, const long long id)
	{
		const auto query = db.CreateQuery(SERIES_QUERY);
		query->Bind(":id", id);
		query->Execute();
		return query->Eof() ? DataItem::Ptr {} : DatabaseUser::CreateSimpleListItem(*query, QUERY_INDEX_SIMPLE_LIST_ITEM);
	}

	static DataItem::Ptr CreateDictionary(DB::IDatabase & db, const char * queryText, const long long id, const Extractor extractor)
	{
		auto root = NavigationItem::Create();
		const auto query = db.CreateQuery(queryText);
		query->Bind(":id", id);
		for (query->Execute(); !query->Eof(); query->Next())
			root->AppendChild(extractor(*query, QUERY_INDEX_SIMPLE_LIST_ITEM));
		return root;
	}

private:
	PropagateConstPtr<Util::IExecutor> m_executor;
	PropagateConstPtr<DatabaseUser, std::shared_ptr> m_databaseUser;
	PropagateConstPtr<QTimer> m_extractInfoTimer { DatabaseUser::CreateTimer([&] { ExtractInfo(); }) };

	QString m_currentBookId;
};

AnnotationController::AnnotationController(const std::shared_ptr<ILogicFactory> & logicFactory
	, std::shared_ptr<DatabaseUser> databaseUser
)
	: m_impl(logicFactory, std::move(databaseUser))
{
	PLOGD << "AnnotationController created";
}
AnnotationController::~AnnotationController()
{
	PLOGD << "AnnotationController destroyed";
}

void AnnotationController::SetCurrentBookId(QString bookId)
{
	m_impl->SetCurrentBookId(std::move(bookId));
}
