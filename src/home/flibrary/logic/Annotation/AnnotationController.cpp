#include "AnnotationController.h"

#include <QDateTime>

#include "fnd/observable.h"
#include "fnd/EnumBitmask.h"

#include "interface/logic/IProgressController.h"

#include "data/DataItem.h"
#include "database/DatabaseUser.h"
#include "database/interface/IQuery.h"
#include "util/UiTimer.h"

#include "ArchiveParser.h"

#include <plog/Log.h>

using namespace HomeCompa;
using namespace Flibrary;

namespace {

using Extractor = IDataItem::Ptr(*)(const DB::IQuery & query, const int * index);
constexpr int QUERY_INDEX_SIMPLE_LIST_ITEM[] { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

constexpr auto BOOK_QUERY = "select %1 from Books b left join Books_User bu on bu.BookID = b.BookID where b.BookID = :id";
constexpr auto SERIES_QUERY = "select s.SeriesID, s.SeriesTitle from Series s join Books b on b.SeriesID = s.seriesID and b.BookID = :id";
constexpr auto AUTHORS_QUERY = "select a.AuthorID, a.LastName, a.LastName, a.FirstName, a.MiddleName from Authors a  join Author_List al on al.AuthorID = a.AuthorID and al.BookID = :id";
constexpr auto GENRES_QUERY = "select g.GenreCode, g.GenreAlias from Genres g join Genre_List gl on gl.GenreCode = g.GenreCode and gl.BookID = :id";
constexpr auto GROUPS_QUERY = "select g.GroupID, g.Title from Groups_User g join Groups_List_User gl on gl.GroupID = g.GroupID and gl.BookID = :id";
constexpr auto KEYWORDS_QUERY = "select k.KeywordID, k.KeywordTitle from Keywords k join Keyword_List kl on kl.KeywordID = k.KeywordID and kl.BookID = :id order by k.KeywordTitle";

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
	, IProgressController::IObserver
{
public:
	explicit Impl(const std::shared_ptr<const ILogicFactory>& logicFactory
		, std::shared_ptr<const DatabaseUser> databaseUser
	)
		: m_logicFactory(logicFactory)
		, m_databaseUser(std::move(databaseUser))
		, m_executor(logicFactory->GetExecutor())
	{
	}

public:
	void SetCurrentBookId(QString bookId)
	{
		Perform(&IAnnotationController::IObserver::OnAnnotationRequested);
		if (m_currentBookId = std::move(bookId); !m_currentBookId.isEmpty())
			m_extractInfoTimer->start();
	}

private: // IDataProvider
	[[nodiscard]] const IDataItem & GetBook() const noexcept override
	{
		return *m_book;
	}

	[[nodiscard]] const IDataItem & GetSeries() const noexcept override
	{
		return *m_series;
	}

	[[nodiscard]] const IDataItem & GetAuthors() const noexcept override
	{
		return *m_authors;
	}

	[[nodiscard]] const IDataItem & GetGenres() const noexcept override
	{
		return *m_genres;
	}

	[[nodiscard]] const IDataItem & GetGroups() const noexcept override
	{
		return *m_groups;
	}

	[[nodiscard]] const IDataItem & GetKeywords() const noexcept override
	{
		return *m_keywords;
	}

	[[nodiscard]] const QString & GetError() const noexcept override
	{
		return m_archiveData.error;
	}

	[[nodiscard]] const QString & GetAnnotation() const noexcept override
	{
		return m_archiveData.annotation;
	}

	[[nodiscard]] const QString & GetEpigraph() const noexcept override
	{
		return m_archiveData.epigraph;
	}

	[[nodiscard]] const QString & GetEpigraphAuthor() const noexcept override
	{
		return m_archiveData.epigraphAuthor;
	}

	[[nodiscard]] const std::vector<QString> & GetFb2Keywords() const noexcept override
	{
		return m_archiveData.keywords;
	}

	[[nodiscard]] const Covers & GetCovers() const noexcept override
	{
		return m_archiveData.covers;
	}

	[[nodiscard]] int GetCoverIndex() const noexcept override
	{
		return m_archiveData.coverIndex;
	}

	[[nodiscard]] size_t GetTextSize() const noexcept override
	{
		return m_archiveData.textSize;
	}

	[[nodiscard]] IDataItem::Ptr GetContent() const noexcept override
	{
		return m_archiveData.content;
	}

	[[nodiscard]] IDataItem::Ptr GetTranslators() const noexcept override
	{
		return m_archiveData.translators;
	}

	[[nodiscard]] const QString & GetPublisher() const noexcept override
	{
		return m_archiveData.publishInfo.publisher;
	}

	[[nodiscard]] const QString & GetPublishCity() const noexcept override
	{
		return m_archiveData.publishInfo.city;
	}

	[[nodiscard]] const QString & GetPublishYear() const noexcept override
	{
		return m_archiveData.publishInfo.year;
	}

	[[nodiscard]] const QString & GetPublishIsbn() const noexcept override
	{
		return m_archiveData.publishInfo.isbn;
	}

	[[nodiscard]] const ExportStatistics & GetExportStatistics() const override
	{
		return m_exportStatistics;
	}

private: // IProgressController::IObserver
	void OnStartedChanged() override
	{
		Perform(&IAnnotationController::IObserver::OnArchiveParserProgress, 0);
	}

	void OnValueChanged() override
	{
		if (const auto progressController = m_archiveParserProgressController.lock())
			Perform(&IAnnotationController::IObserver::OnArchiveParserProgress, static_cast<int>(std::lround(progressController->GetValue() * 100)));
	}

	void OnStop() override
	{
		Perform(&IAnnotationController::IObserver::OnArchiveParserProgress, 100);
	}

private:
	void ExtractInfo()
	{
		m_ready = Ready::None;
		m_databaseUser->Execute({ "Get database book info", [&, id = m_currentBookId.toLongLong()]
		{
			const auto db = m_databaseUser->Database();
			return [this, book = CreateBook(*db, id)] (size_t) mutable
			{
				if (book->GetId() == m_currentBookId)
					ExtractInfo(std::move(book));
			};
		} }, 3);
	}

	void ExtractInfo(IDataItem::Ptr book)
	{
		ExtractArchiveInfo(book);
		ExtractDatabaseInfo(std::move(book));
	}

	void ExtractArchiveInfo(IDataItem::Ptr book)
	{
		if (const auto progressController = m_archiveParserProgressController.lock())
			progressController->Stop();

		auto parser = ILogicFactory::Lock(m_logicFactory)->CreateArchiveParser();

		(*m_executor)({ "Get archive book info", [this, book = std::move(book), parser = std::move(parser)] () mutable
		{
			const auto progressController = parser->GetProgressController();
			progressController->RegisterObserver(this);
			m_archiveParserProgressController = progressController;
			auto data = parser->Parse(*book);
			return [this, book = std::move(book), data = std::move(data)] (size_t) mutable
			{
				if (book->GetId() != m_currentBookId)
					return;

				m_archiveData = std::move(data);
				m_ready |= Ready::Archive;

				if (m_ready == Ready::All)
					Perform(&IAnnotationController::IObserver::OnAnnotationChanged, std::cref(*this));
			};
		} });
	}

	void ExtractDatabaseInfo(IDataItem::Ptr book)
	{
		m_databaseUser->Execute({ "Get database book additional info", [this, book = std::move(book)] () mutable
		{
			const auto db = m_databaseUser->Database();
			const auto bookId = book->GetId().toLongLong();
			auto series = CreateSeries(*db, bookId);
			auto authors = CreateDictionary(*db, AUTHORS_QUERY, bookId, &DatabaseUser::CreateFullAuthorItem);
			auto genres = CreateDictionary(*db, GENRES_QUERY, bookId, &DatabaseUser::CreateSimpleListItem);
			auto groups = CreateDictionary(*db, GROUPS_QUERY, bookId, &DatabaseUser::CreateSimpleListItem);
			auto keywords = CreateDictionary(*db, KEYWORDS_QUERY, bookId, &DatabaseUser::CreateSimpleListItem);

			std::unordered_map<ExportStat::Type, std::vector<QDateTime>> exportStatisticsBuffer;
			const auto query = db->CreateQuery("select ExportType, CreatedAt from Export_List where BookID = ?");
			for (query->Bind(0, bookId), query->Execute(); !query->Eof(); query->Next())
				exportStatisticsBuffer[static_cast<ExportStat::Type>(query->Get<int>(0))].push_back(QDateTime::fromString(query->Get<const char*>(1), Qt::ISODate));

			ExportStatistics exportStatistics;
			std::ranges::move(exportStatisticsBuffer, std::back_inserter(exportStatistics));

			return [this
				, book = std::move(book)
				, series = std::move(series)
				, authors = std::move(authors)
				, genres = std::move(genres)
				, groups = std::move(groups)
				, keywords = std::move(keywords)
				, exportStatistics = std::move(exportStatistics)
			] (size_t) mutable
			{
				if (book->GetId() != m_currentBookId)
					return;

				m_book = std::move(book);
				m_series = std::move(series);
				m_authors = std::move(authors);
				m_genres = std::move(genres);
				m_groups = std::move(groups);
				m_keywords = std::move(keywords);
				m_exportStatistics = std::move(exportStatistics);
				m_ready |= Ready::Database;

				if (m_ready == Ready::All)
					Perform(&IAnnotationController::IObserver::OnAnnotationChanged, std::cref(*this));
			};
		} }, 3);
	}

	static IDataItem::Ptr CreateBook(DB::IDatabase & db, const long long id)
	{
		const auto query = db.CreateQuery(QString(BOOK_QUERY).arg(DatabaseUser::BOOKS_QUERY_FIELDS).toStdString());
		query->Bind(":id", id);
		query->Execute();
		return query->Eof() ? NavigationItem::Create() : DatabaseUser::CreateBookItem(*query);
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
	std::weak_ptr<const ILogicFactory> m_logicFactory;
	std::shared_ptr<const DatabaseUser> m_databaseUser;
	PropagateConstPtr<Util::IExecutor> m_executor;
	PropagateConstPtr<QTimer> m_extractInfoTimer { Util::CreateUiTimer([&] { ExtractInfo(); }) };

	QString m_currentBookId;

	Ready m_ready { Ready::None };

	std::weak_ptr<IProgressController> m_archiveParserProgressController;
	ArchiveParser::Data m_archiveData;

	IDataItem::Ptr m_book;
	IDataItem::Ptr m_series;
	IDataItem::Ptr m_authors;
	IDataItem::Ptr m_genres;
	IDataItem::Ptr m_groups;
	IDataItem::Ptr m_keywords;

	ExportStatistics m_exportStatistics;
};

AnnotationController::AnnotationController(const std::shared_ptr<const ILogicFactory>& logicFactory
	, std::shared_ptr<DatabaseUser> databaseUser
)
	: m_impl(logicFactory
		, std::move(databaseUser)
	)
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
