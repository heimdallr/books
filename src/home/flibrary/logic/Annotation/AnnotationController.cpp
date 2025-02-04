#include "AnnotationController.h"

#include <ranges>

#include <QBuffer>
#include <QDateTime>
#include <QFileInfo>
#include <QPixmap>
#include <QTimer>

#include "fnd/observable.h"
#include "fnd/EnumBitmask.h"

#include "interface/constants/ExportStat.h"
#include "interface/constants/Localization.h"
#include "interface/constants/ProductConstant.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/logic/IProgressController.h"

#include "data/DataItem.h"
#include "database/DatabaseUtil.h"
#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"
#include "util/localization.h"
#include "util/UiTimer.h"

#include "ArchiveParser.h"

#include <plog/Log.h>

using namespace HomeCompa;
using namespace Flibrary;

namespace {

constexpr auto CONTEXT = "Annotation";
constexpr auto KEYWORDS_FB2 = QT_TRANSLATE_NOOP("Annotation", "Keywords: %1");
constexpr auto FILENAME = QT_TRANSLATE_NOOP("Annotation", "File:");
constexpr auto SIZE = QT_TRANSLATE_NOOP("Annotation", "Size:");
constexpr auto IMAGES = QT_TRANSLATE_NOOP("Annotation", "Images:");
constexpr auto UPDATED = QT_TRANSLATE_NOOP("Annotation", "Updated:");
constexpr auto TRANSLATORS = QT_TRANSLATE_NOOP("Annotation", "Translators:");
constexpr auto TEXT_SIZE = QT_TRANSLATE_NOOP("Annotation", "%L1 (%2%3 pages)");
constexpr auto EXPORT_STATISTICS = QT_TRANSLATE_NOOP("Annotation", "Export statistics:");

TR_DEF

using Extractor = IDataItem::Ptr(*)(const DB::IQuery & query, const size_t * index);
constexpr size_t QUERY_INDEX_SIMPLE_LIST_ITEM[] { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

constexpr auto BOOK_QUERY = "select %1 from Books b join Folders f on f.FolderID = b.FolderID left join Books_User bu on bu.BookID = b.BookID where b.BookID = :id";
constexpr auto SERIES_QUERY = "select s.SeriesID, s.SeriesTitle from Series s join Books b on b.SeriesID = s.seriesID and b.BookID = :id";
constexpr auto AUTHORS_QUERY = "select a.AuthorID, a.LastName, a.LastName, a.FirstName, a.MiddleName from Authors a  join Author_List al on al.AuthorID = a.AuthorID and al.BookID = :id";
constexpr auto GENRES_QUERY = "select g.GenreCode, g.GenreAlias from Genres g join Genre_List gl on gl.GenreCode = g.GenreCode and gl.BookID = :id";
constexpr auto GROUPS_QUERY = "select g.GroupID, g.Title from Groups_User g join Groups_List_User gl on gl.GroupID = g.GroupID and gl.BookID = :id";
constexpr auto KEYWORDS_QUERY = "select k.KeywordID, k.KeywordTitle from Keywords k join Keyword_List kl on kl.KeywordID = k.KeywordID and kl.BookID = :id order by k.KeywordTitle";

constexpr auto ERROR_PATTERN = R"(<p style="font-style:italic;">%1</p>)";
constexpr auto TITLE_PATTERN = "<p align=center><b>%1</b></p>";
constexpr auto EPIGRAPH_PATTERN = R"(<p align=right style="font-style:italic;">%1</p>)";

enum class Ready
{
	None     = 0,
	Database = 1 << 0,
	Archive  = 1 << 1,
	All = None
		| Database
		| Archive
};

template<typename T>
T Round(const T value, const int digits)
{
	if (value == 0)
		return 0;

	const double factor = pow(10.0, digits + ceil(log10(value)));
	if (factor < 10.0)
		return value;

	return static_cast<T>(static_cast<int64_t>(value / factor + 0.5) * factor + 0.5);
}

QString Url(const char * type, const QString & id, const QString & str)
{
	return str.isEmpty() ? QString {} : QString("<a href=%1//%2>%3</a>").arg(type, id, str);
}

QString GetTitle(const IDataItem & item)
{
	return item.GetData(DataItem::Column::Title);
}

QString GetTitleAuthor(const IDataItem & item)
{
	auto result = item.GetData(AuthorItem::Column::LastName);
	AppendTitle(result, item.GetData(AuthorItem::Column::FirstName));
	AppendTitle(result, item.GetData(AuthorItem::Column::MiddleName));
	return result;
}

void Add(QString & text, const QString & str, const char * pattern = "%1")
{
	if (!str.isEmpty())
		text.append(QString("<p>%1</p>").arg(Tr(pattern).arg(str)));
}

using TitleGetter = QString(*)(const IDataItem & item);

QString Join(const std::vector<QString> & strings, const QString & delimiter = ", ")
{
	if (strings.empty())
		return {};

	auto result = strings.front();
	std::ranges::for_each(strings | std::views::drop(1), [&] (const QString & item)
	{
		result.append(delimiter).append(item);
	});

	return result;
}

QString Urls(const char * type, const IDataItem & parent, const TitleGetter tileGetter = &GetTitle)
{
	std::vector<QString> urls;
	for (size_t i = 0, sz = parent.GetChildCount(); i < sz; ++i)
	{
		const auto item = parent.GetChild(i);
		urls.emplace_back(Url(type, item->GetId(), tileGetter(*item)));
	}

	return Join(urls);
}

QString GetPublishInfo(const IAnnotationController::IDataProvider & dataProvider)
{
	QString result = dataProvider.GetPublisher();
	AppendTitle(result, dataProvider.GetPublishCity(), ", ");
	AppendTitle(result, dataProvider.GetPublishYear(), ", ");
	const auto isbn = dataProvider.GetPublishIsbn().isEmpty() ? QString {} : QString("ISBN %1").arg(dataProvider.GetPublishIsbn());
	AppendTitle(result, isbn, ". ");
	return result;
}

struct Table
{
	Table & Add(const char * name, const QString & value)
	{
		if (!value.isEmpty())
			data << QString(R"(<tr><td>%1</td><td>%2</td></tr>)").arg(Tr(name)).arg(value);

		return *this;
	}

	[[nodiscard]] QString ToString() const
	{
		return data.isEmpty() ? QString {} : QString("<table>%1</table>\n").arg(data.join("\n"));
	}

	QStringList data;
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
		, std::shared_ptr<const IDatabaseUser> databaseUser
	)
		: m_logicFactory { logicFactory }
		, m_databaseUser { std::move(databaseUser) }
		, m_executor { logicFactory->GetExecutor() }
	{
	}

public:
	void SetCurrentBookId(QString bookId, const bool extractNow)
	{
		Perform(&IAnnotationController::IObserver::OnAnnotationRequested);
		if (m_currentBookId = std::move(bookId); !m_currentBookId.isEmpty())
			extractNow ? ExtractInfo() : m_extractInfoTimer->start();
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

	[[nodiscard]] std::optional<size_t> GetCoverIndex() const noexcept override
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
		if (QFileInfo(book->GetRawData(BookItem::Column::FileName)).suffix().compare("fb2", Qt::CaseInsensitive))
		{
			m_book = std::move(book);
			m_ready |= Ready::Archive;
			return;
		}

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
			auto authors = CreateDictionary(*db, AUTHORS_QUERY, bookId, &DatabaseUtil::CreateFullAuthorItem);
			auto genres = CreateDictionary(*db, GENRES_QUERY, bookId, &DatabaseUtil::CreateSimpleListItem);
			auto groups = CreateDictionary(*db, GROUPS_QUERY, bookId, &DatabaseUtil::CreateSimpleListItem);
			auto keywords = CreateDictionary(*db, KEYWORDS_QUERY, bookId, &DatabaseUtil::CreateSimpleListItem);

			std::unordered_map<ExportStat::Type, std::vector<QDateTime>> exportStatisticsBuffer;
			const auto query = db->CreateQuery("select ExportType, CreatedAt from Export_List_User where BookID = ?");
			for (query->Bind(0, bookId), query->Execute(); !query->Eof(); query->Next())
				exportStatisticsBuffer[static_cast<ExportStat::Type>(query->Get<int>(0))].emplace_back(QDateTime::fromString(query->Get<const char*>(1), Qt::ISODate));

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
		const auto query = db.CreateQuery(QString(BOOK_QUERY).arg(IDatabaseUser::BOOKS_QUERY_FIELDS).toStdString());
		query->Bind(":id", id);
		query->Execute();
		return query->Eof() ? NavigationItem::Create() : DatabaseUtil::CreateBookItem(*query);
	}

	static IDataItem::Ptr CreateSeries(DB::IDatabase & db, const long long id)
	{
		const auto query = db.CreateQuery(SERIES_QUERY);
		query->Bind(":id", id);
		query->Execute();
		return query->Eof() ? NavigationItem::Create() : DatabaseUtil::CreateSimpleListItem(*query, QUERY_INDEX_SIMPLE_LIST_ITEM);
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
	std::shared_ptr<const IDatabaseUser> m_databaseUser;
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
	, std::shared_ptr<IDatabaseUser> databaseUser
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

void AnnotationController::SetCurrentBookId(QString bookId, const bool extractNow)
{
	m_impl->SetCurrentBookId(std::move(bookId), extractNow);
}

QString AnnotationController::CreateAnnotation(const IDataProvider & dataProvider) const
{
	const auto & book = dataProvider.GetBook();
	QString annotation;
	Add(annotation, Url(Constant::BOOK, book.GetId(), book.GetRawData(BookItem::Column::Title)), TITLE_PATTERN);
	Add(annotation, dataProvider.GetEpigraph(), EPIGRAPH_PATTERN);
	Add(annotation, dataProvider.GetEpigraphAuthor(), EPIGRAPH_PATTERN);
	Add(annotation, dataProvider.GetAnnotation());

	auto & keywords = dataProvider.GetKeywords();
	if (keywords.GetChildCount() == 0)
		Add(annotation, Join(dataProvider.GetFb2Keywords()), KEYWORDS_FB2);

	const auto & folder = book.GetRawData(BookItem::Column::Folder);

	Add(annotation, Table()
		.Add(Loc::AUTHORS, Urls(Loc::AUTHORS, dataProvider.GetAuthors(), &GetTitleAuthor))
		.Add(Loc::SERIES, Url(Loc::SERIES, dataProvider.GetSeries().GetId(), dataProvider.GetSeries().GetRawData(NavigationItem::Column::Title)))
		.Add(Loc::GENRES, Urls(Loc::GENRES, dataProvider.GetGenres()))
		.Add(Loc::ARCHIVE, Url(Loc::ARCHIVE, book.GetRawData(BookItem::Column::FolderID), folder))
		.Add(Loc::GROUPS, Urls(Loc::GROUPS, dataProvider.GetGroups()))
		.Add(Loc::KEYWORDS, Urls(Loc::KEYWORDS, keywords))
		.ToString());

	if (const auto translators = dataProvider.GetTranslators(); translators && translators->GetChildCount() > 0)
	{
		Table table;
		for (size_t i = 0, sz = translators->GetChildCount(); i < sz; ++i)
			table.Add(i == 0 ? TRANSLATORS : "", GetAuthorFull(*translators->GetChild(i)));
		Add(annotation, table.ToString());
	}

	{
		const auto addRate = [&] (Table & info, const char * name, const int column)
		{
			const auto rate = book.GetRawData(column).toInt();
			if (rate > 0 && rate <= 5)
				info.Add(name, QString("@%1@").arg(name));
		};

		auto info = Table().Add(FILENAME, book.GetRawData(BookItem::Column::FileName));
		if (dataProvider.GetTextSize() > 0)
			info.Add(SIZE, Tr(TEXT_SIZE).arg(dataProvider.GetTextSize()).arg(QChar(0x2248)).arg(std::max(1ULL, Round(dataProvider.GetTextSize() / 2000, -2))));
		info.Add(UPDATED, book.GetRawData(BookItem::Column::UpdateDate));
		addRate(info, Loc::RATE, BookItem::Column::LibRate);
		addRate(info, Loc::USER_RATE, BookItem::Column::UserRate);

		if (const auto & covers = dataProvider.GetCovers(); !covers.empty())
		{
			const auto total = std::accumulate(covers.cbegin(), covers.cend(), qsizetype { 0 }, [] (const auto init, const auto & cover)
			{
				return init + cover.bytes.size();
			});
			info.Add(IMAGES, QString("%1 (%L2)").arg(covers.size()).arg(total));
		}

		Add(annotation, info.ToString());
	}

	Add(annotation, GetPublishInfo(dataProvider));

	Add(annotation, dataProvider.GetError(), ERROR_PATTERN);

	if (!dataProvider.GetExportStatistics().empty())
	{
		const auto toDateList = [] (const std::vector<QDateTime> & dates)
		{
			QStringList result;
			result.reserve(static_cast<int>(dates.size()));
			std::ranges::transform(dates, std::back_inserter(result), [] (const QDateTime & date)
			{
				return date.toString("yy.MM.dd hh:mm");
			});
			return result.join(", ");
		};
		auto exportStatistics = Table().Add(EXPORT_STATISTICS, " ");
		for (const auto & [type, dates] : dataProvider.GetExportStatistics())
			exportStatistics.Add(GetName(type), toDateList(dates));
		Add(annotation, exportStatistics.ToString());
	}

	return annotation;
}

void AnnotationController::RegisterObserver(IObserver * observer)
{
	m_impl->Register(observer);
}

void AnnotationController::UnregisterObserver(IObserver * observer)
{
	m_impl->Unregister(observer);
}
