#include "AnnotationController.h"

#include <plog/Log.h>

#include "fnd/observable.h"
#include "fnd/EnumBitmask.h"

#include "database/interface/IQuery.h"

#include "ArchiveParser.h"
#include "data/DataItem.h"
#include "shared/DatabaseUser.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace {

using Extractor = IDataItem::Ptr(*)(const DB::IQuery & query, const int * index);
constexpr int QUERY_INDEX_SIMPLE_LIST_ITEM[] { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

constexpr auto BOOK_QUERY = "select %1 from Books b left join Books_User bu on bu.BookID = b.BookID where b.BookID = :id";
constexpr auto SERIES_QUERY = "select s.SeriesID, s.SeriesTitle from Series s join Books b on b.SeriesID = s.seriesID and b.BookID = :id";
constexpr auto AUTHORS_QUERY = "select a.AuthorID, a.LastName, a.FirstName, a.MiddleName from Authors a  join Author_List al on al.AuthorID = a.AuthorID and al.BookID = :id";
constexpr auto GENRES_QUERY = "select g.GenreCode, g.GenreAlias from Genres g join Genre_List gl on gl.GenreCode = g.GenreCode and gl.BookID = :id";
constexpr auto GROUPS_QUERY = "select g.GroupID, g.Title from Groups_User g join Groups_List_User gl on gl.GroupID = g.GroupID and gl.BookID = :id";

enum class Ready
{
	None     = 0,
	Database = 1 << 0,
	Archive  = 1 << 1,
	All = None
		| Database
		| Archive
};

}

ENABLE_BITMASK_OPERATORS(Ready);

class AnnotationController::Impl final
	: public Observable<IObserver>
	, public IDataProvider
{
public:
	explicit Impl(std::shared_ptr<ILogicFactory> logicFactory
		, std::shared_ptr<DatabaseUser> databaseUser
	)
		: m_logicFactory(std::move(logicFactory))
		, m_databaseUser(std::move(databaseUser))
		, m_executor(m_logicFactory->GetExecutor())
	{
	}

public:
	void SetCurrentBookId(QString bookId)
	{
		m_currentBookId = std::move(bookId);
		m_extractInfoTimer->start();
	}

private: // IDataProvider
	[[nodiscard]] const IDataItem & GatBook() const noexcept override
	{
		return *m_book;
	}

	[[nodiscard]] const IDataItem & GatSeries() const noexcept override
	{
		return *m_series;
	}

	[[nodiscard]] const IDataItem & GatAuthors() const noexcept override
	{
		return *m_authors;
	}

	[[nodiscard]] const IDataItem & GatGenres() const noexcept override
	{
		return *m_genres;
	}

	[[nodiscard]] const IDataItem & GatGroups() const noexcept override
	{
		return *m_groups;
	}

	[[nodiscard]] const QString & GetAnnotation() const noexcept override
	{
		return m_archiveData.annotation;
	}

	[[nodiscard]] const std::vector<QString> & GetKeywords() const noexcept override
	{
		return m_archiveData.keywords;
	}

	[[nodiscard]] const std::vector<QByteArray> & GetCovers() const noexcept override
	{
		return m_archiveData.covers;
	}

	[[nodiscard]] int GetCoverIndex() const noexcept override
	{
		return m_archiveData.coverIndex;
	}

	[[nodiscard]] const IDataItem & GetContent() const noexcept override
	{
		return *m_archiveData.content;
	}

private:
	void ExtractInfo()
	{
		Perform(&IObserver::OnAnnotationRequested);
		m_databaseUser->Execute({ "Get database book info", [&, id = m_currentBookId.toLongLong()]
		{
			const auto db = m_databaseUser->Database();
			return [&, book = CreateBook(*db, id)] (size_t) mutable
			{
				if (book->GetId() == m_currentBookId)
					ExtractInfo(std::move(book));
			};
		} }, 3);
	}

	void ExtractInfo(IDataItem::Ptr book)
	{
		m_ready = Ready::None;

		ExtractArchiveInfo(book);
		ExtractDatabaseInfo(std::move(book));
	}

	void ExtractArchiveInfo(IDataItem::Ptr book)
	{
		(*m_executor)({ "Get archive book info", [&, book = std::move(book)]
		{
			return [&, data = m_logicFactory->CreateArchiveParser()->Parse(*book)] (size_t) mutable
			{
				if (book->GetId() != m_currentBookId)
					return;

				m_archiveData = std::move(data);

				if ((m_ready |= Ready::Archive) == Ready::All)
					Perform(&IObserver::OnAnnotationChanged, std::cref(*this));
			};
		} });
	}

	void ExtractDatabaseInfo(IDataItem::Ptr book)
	{
		m_databaseUser->Execute({ "Get database book additional info", [&, book = std::move(book)] () mutable
		{
			const auto db = m_databaseUser->Database();
			const auto bookId = book->GetId().toLongLong();
			auto series = CreateSeries(*db, bookId);
			auto authors = CreateDictionary(*db, AUTHORS_QUERY, bookId, &DatabaseUser::CreateFullAuthorItem);
			auto genres = CreateDictionary(*db, GENRES_QUERY, bookId, &DatabaseUser::CreateSimpleListItem);
			auto groups = CreateDictionary(*db, GROUPS_QUERY, bookId, &DatabaseUser::CreateSimpleListItem);
			return [&, book = std::move(book), series = std::move(series), authors = std::move(authors), genres = std::move(genres), groups = std::move(groups)] (size_t) mutable
			{
				if (book->GetId() != m_currentBookId)
					return;

				m_book = std::move(book);
				m_series = std::move(series);
				m_authors = std::move(authors);
				m_genres = std::move(genres);
				m_groups = std::move(groups);

				if ((m_ready |= Ready::Database) == Ready::All)
					Perform(&IObserver::OnAnnotationChanged, std::cref(*this));
			};
		} }, 3);
	}

	static IDataItem::Ptr CreateBook(DB::IDatabase & db, const long long id)
	{
		const auto query = db.CreateQuery(QString(BOOK_QUERY).arg(DatabaseUser::BOOKS_QUERY_FIELDS).toStdString());
		query->Bind(":id", id);
		query->Execute();
		assert(!query->Eof());
		return DatabaseUser::CreateBookItem(*query);
	}

	static IDataItem::Ptr CreateSeries(DB::IDatabase & db, const long long id)
	{
		const auto query = db.CreateQuery(SERIES_QUERY);
		query->Bind(":id", id);
		query->Execute();
		return query->Eof() ? NavigationItem::Create() : DatabaseUser::CreateSimpleListItem(*query, QUERY_INDEX_SIMPLE_LIST_ITEM);
	}

	static IDataItem::Ptr CreateDictionary(DB::IDatabase & db, const char * queryText, const long long id, const Extractor extractor)
	{
		auto root = NavigationItem::Create();
		const auto query = db.CreateQuery(queryText);
		query->Bind(":id", id);
		for (query->Execute(); !query->Eof(); query->Next())
		{
			auto child = extractor(*query, QUERY_INDEX_SIMPLE_LIST_ITEM);
			child->Reduce();
			root->AppendChild(std::move(child));
		}

		return root;
	}

private:
	PropagateConstPtr<ILogicFactory, std::shared_ptr> m_logicFactory;
	PropagateConstPtr<DatabaseUser, std::shared_ptr> m_databaseUser;
	PropagateConstPtr<Util::IExecutor> m_executor;
	PropagateConstPtr<QTimer> m_extractInfoTimer { DatabaseUser::CreateTimer([&] { ExtractInfo(); }) };

	QString m_currentBookId;

	Ready m_ready { Ready::None };

	ArchiveParser::Data m_archiveData;

	IDataItem::Ptr m_book;
	IDataItem::Ptr m_series;
	IDataItem::Ptr m_authors;
	IDataItem::Ptr m_genres;
	IDataItem::Ptr m_groups;
};

AnnotationController::AnnotationController(std::shared_ptr<ILogicFactory> logicFactory
	, std::shared_ptr<DatabaseUser> databaseUser
)
	: m_impl(std::move(logicFactory), std::move(databaseUser))
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

void AnnotationController::RegisterObserver(IObserver * observer)
{
	m_impl->Register(observer);
}

void AnnotationController::UnregisterObserver(IObserver * observer)
{
	m_impl->Unregister(observer);
}
