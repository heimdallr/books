#include "AnnotationController.h"

#include <random>
#include <ranges>

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPixmap>
#include <QTimer>

#include "fnd/EnumBitmask.h"
#include "fnd/FindPair.h"
#include "fnd/observable.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/constants/ExportStat.h"
#include "interface/constants/Localization.h"
#include "interface/constants/ProductConstant.h"
#include "interface/logic/IFilterProvider.h"
#include "interface/logic/IJokeRequester.h"
#include "interface/logic/IProgressController.h"

#include "data/DataItem.h"
#include "database/DatabaseUtil.h"
#include "inpx/constant.h"
#include "util/UiTimer.h"
#include "util/localization.h"

#include "ArchiveParser.h"
#include "log.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto CONTEXT           = "Annotation";
constexpr auto KEYWORDS_FB2      = QT_TRANSLATE_NOOP("Annotation", "Keywords: %1");
constexpr auto FILENAME          = QT_TRANSLATE_NOOP("Annotation", "File:");
constexpr auto BOOK_SIZE         = QT_TRANSLATE_NOOP("Annotation", "Size:");
constexpr auto IMAGES            = QT_TRANSLATE_NOOP("Annotation", "Images:");
constexpr auto TRANSLATORS       = QT_TRANSLATE_NOOP("Annotation", "Translators:");
constexpr auto TEXT_SIZE         = QT_TRANSLATE_NOOP("Annotation", "%L1 letters (%2%3 pages, %2%L4 words)");
constexpr auto EXPORT_STATISTICS = QT_TRANSLATE_NOOP("Annotation", "Export statistics:");
constexpr auto OR                = QT_TRANSLATE_NOOP("Annotation", " or %1");
constexpr auto TRANSLATION_FROM  = QT_TRANSLATE_NOOP("Annotation", ", translated from %1");
constexpr auto REVIEWS           = QT_TRANSLATE_NOOP("Annotation", "Readers' Reviews");

TR_DEF

using Extractor = IDataItem::Ptr (*)(const DB::IQuery& query);

constexpr auto SERIES_QUERY = "select s.SeriesID, s.SeriesTitle, sl.SeqNumber from Series s join Series_List sl on sl.SeriesID = s.SeriesID and sl.BookID = :id where s.Flags & {} = 0 order by sl.OrdNum";
constexpr auto AUTHORS_QUERY =
	"select a.AuthorID, a.LastName, a.FirstName, a.MiddleName from Authors a join Author_List al on al.AuthorID = a.AuthorID and al.BookID = :id where a.Flags & {} = 0 order by al.OrdNum";
constexpr auto GENRES_QUERY   = "select g.GenreCode, g.GenreAlias from Genres g join Genre_List gl on gl.GenreCode = g.GenreCode and gl.BookID = :id  where g.Flags & {} = 0 order by gl.OrdNum";
constexpr auto GROUPS_QUERY   = "select g.GroupID, g.Title from Groups_User g join Groups_List_User_View gl on gl.GroupID = g.GroupID and gl.BookID = :id";
constexpr auto KEYWORDS_QUERY = "select k.KeywordID, k.KeywordTitle from Keywords k join Keyword_List kl on kl.KeywordID = k.KeywordID and kl.BookID = :id where k.Flags & {} = 0 order by kl.OrdNum";
constexpr auto REVIEWS_QUERY  = "select b.LibID, r.Folder from Reviews r join Books b on b.BookID = r.BookID where r.BookID = :id";
constexpr auto FOLDER_QUERY   = "select f.FolderID, f.FolderTitle from Folders f join Books b on b.FolderID = f.FolderID and b.BookID = :id";
constexpr auto UPDATE_QUERY   = "select u.UpdateID, b.UpdateDate from Updates u join Books b on b.UpdateID = u.UpdateID and b.BookID = :id";

constexpr auto ERROR_PATTERN    = R"(<p style="font-style:italic;">%1</p>)";
constexpr auto TITLE_PATTERN    = "<p align=center><b>%1</b></p>";
constexpr auto EPIGRAPH_PATTERN = R"(<p align=right style="font-style:italic;">%1</p>)";

enum class Ready
{
	None     = 0,
	Database = 1 << 0,
	Archive  = 1 << 1,
	All      = None | Database | Archive
};

template <typename T>
T Round(const T value, const int digits)
{
	if (value == 0)
		return 0;

	const double factor = pow(10.0, digits + ceil(log10(value)));
	if (factor < 10.0)
		return value;

	return static_cast<T>(static_cast<int64_t>(value / factor + 0.5) * factor + 0.5);
}

QString GetTitle(const IDataItem& item)
{
	return item.GetData(DataItem::Column::Title);
}

QString GetTitleAuthor(const IDataItem& item)
{
	auto result = item.GetData(AuthorItem::Column::LastName);
	AppendTitle(result, item.GetData(AuthorItem::Column::FirstName));
	AppendTitle(result, item.GetData(AuthorItem::Column::MiddleName));
	return result;
}

using TitleGetter = QString (*)(const IDataItem& item);

QString Join(const std::vector<QString>& strings, const QString& delimiter = ", ")
{
	if (strings.empty())
		return {};

	auto result = strings.front();
	std::ranges::for_each(strings | std::views::drop(1), [&](const QString& item) {
		result.append(delimiter).append(item);
	});

	return result;
}

QString Urls(const IAnnotationController::IStrategy& strategy, const char* type, const IDataItem& parent, const TitleGetter titleGetter = &GetTitle)
{
	std::vector<QString> urls;
	for (size_t i = 0, sz = parent.GetChildCount(); i < sz; ++i)
	{
		const auto item = parent.GetChild(i);
		urls.emplace_back(strategy.GenerateUrl(type, item->GetId(), titleGetter(*item)));
	}

	return Join(urls);
}

QString GetPublishInfo(const IAnnotationController::IDataProvider& dataProvider)
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
	explicit Table(const IAnnotationController::IStrategy& strategy)
		: m_strategy { strategy }
	{
	}

	Table& Add(const char* name, const QString& value)
	{
		if (!value.isEmpty())
			m_data << m_strategy.AddTableRow(name, value);

		return *this;
	}

	Table& Add(const QStringList& values)
	{
		m_data << m_strategy.AddTableRow(values);
		return *this;
	}

	[[nodiscard]] QString ToString() const
	{
		return m_strategy.TableRowsToString(m_data);
	}

private:
	QStringList                             m_data;
	const IAnnotationController::IStrategy& m_strategy;
};

QString TranslateLang(const QString& code)
{
	const auto  it       = std::ranges::find(LANGUAGES, code, [](const auto& item) {
        return item.key;
    });
	const auto* language = it != std::end(LANGUAGES) ? it->title : UNDEFINED;
	return Loc::Tr(LANGUAGES_CONTEXT, language);
}

Table CreateUrlTable(const IAnnotationController::IDataProvider& dataProvider, const IAnnotationController::IStrategy& strategy)
{
	const auto& book         = dataProvider.GetBook();
	const auto& keywords     = dataProvider.GetKeywords();
	const auto& lang         = book.GetRawData(BookItem::Column::Lang);
	const auto& fbLang       = dataProvider.GetLanguage();
	const auto& fbSourceLang = dataProvider.GetSourceLanguage();

	const auto langFlags = dataProvider.GetFlags(NavigationMode::Languages, { lang, fbLang, fbSourceLang });

	const auto langTr  = TranslateLang(lang);
	auto       langStr = strategy.GenerateUrl(Loc::LANGUAGE, lang, langTr, !!(langFlags[0] & IDataItem::Flags::Filtered));

	if (!fbLang.isEmpty())
		if (const auto fbLangTr = TranslateLang(fbLang); fbLangTr != langTr && fbLangTr != Loc::Tr(LANGUAGES_CONTEXT, UNDEFINED))
			langStr.append(Tr(OR).arg(strategy.GenerateUrl(Loc::LANGUAGE, fbLang, fbLangTr, !!(langFlags[1] & IDataItem::Flags::Filtered))));

	if (!fbSourceLang.isEmpty() && fbSourceLang != lang && fbSourceLang != fbLang)
		if (const auto fbSourceLangTr = TranslateLang(fbSourceLang); fbSourceLangTr != langTr)
			langStr.append(Tr(TRANSLATION_FROM).arg(strategy.GenerateUrl(Loc::LANGUAGE, fbSourceLang, fbSourceLangTr, !!(langFlags[2] & IDataItem::Flags::Filtered))));

	Table table(strategy);
	table.Add(Loc::AUTHORS, Urls(strategy, Loc::AUTHORS, dataProvider.GetAuthors(), &GetTitleAuthor))
		.Add(Loc::SERIES, Urls(strategy, Loc::SERIES, dataProvider.GetSeries()))
		.Add(Loc::GENRES, Urls(strategy, Loc::GENRES, dataProvider.GetGenres()))
		.Add(Loc::PUBLISH_YEAR, strategy.GenerateUrl(Loc::PUBLISH_YEAR, book.GetRawData(BookItem::Column::Year), book.GetRawData(BookItem::Column::Year)))
		.Add(Loc::ARCHIVE, Urls(strategy, Loc::ARCHIVE, dataProvider.GetFolder()))
		.Add(Loc::GROUPS, Urls(strategy, Loc::GROUPS, dataProvider.GetGroups()))
		.Add(Loc::KEYWORDS, Urls(strategy, Loc::KEYWORDS, keywords))
		.Add(Loc::LANGUAGE, langStr)
		.Add(Loc::UPDATES, Urls(strategy, Loc::UPDATES, dataProvider.GetUpdate()));

	return table;
}

class JokeRequesterClientImpl : virtual public IJokeRequester::IClient
{
public:
	static std::shared_ptr<IClient> Create(IClient& impl)
	{
		return std::make_shared<JokeRequesterClientImpl>(impl);
	}

public:
	explicit JokeRequesterClientImpl(IClient& impl)
		: m_impl(impl)
	{
	}

private: // IJokeRequester::IClient
	void OnTextReceived(const QString& value) override
	{
		m_impl.OnTextReceived(value);
	}

	void OnImageReceived(const QByteArray& value) override
	{
		m_impl.OnImageReceived(value);
	}

private:
	IClient& m_impl;
};

template <typename T>
struct TimeProj
{
	constexpr QDateTime operator()(const T& obj) noexcept
	{
		return obj.time;
	}
};

template <typename T>
struct NameProj
{
	constexpr QString operator()(const T& obj) noexcept
	{
		return obj.name;
	}
};

template <typename T>
struct TextProj
{
	constexpr QString operator()(const T& obj) noexcept
	{
		return obj.text;
	}
};

template <typename Comp, typename Proj>
void SortReviews(IAnnotationController::IDataProvider::Reviews& reviews)
{
	std::ranges::sort(reviews, Comp {}, Proj {});
}

constexpr std::pair<const char*, void (*)(IAnnotationController::IDataProvider::Reviews&)> REVIEW_SORTERS[] {
	{		 "Time",    &SortReviews<std::less<QDateTime>, TimeProj<IAnnotationController::IDataProvider::Review>> },
	{     "TimeDesc", &SortReviews<std::greater<QDateTime>, TimeProj<IAnnotationController::IDataProvider::Review>> },
	{     "Reviewer",      &SortReviews<std::less<QString>, NameProj<IAnnotationController::IDataProvider::Review>> },
	{ "ReviewerDesc",   &SortReviews<std::greater<QString>, NameProj<IAnnotationController::IDataProvider::Review>> },
	{		 "Text",      &SortReviews<std::less<QString>, TextProj<IAnnotationController::IDataProvider::Review>> },
	{     "TextDesc",   &SortReviews<std::greater<QString>, TextProj<IAnnotationController::IDataProvider::Review>> },
};
constexpr auto REVIEW_SORTER_DEFAULT = REVIEW_SORTERS[0].second;

} // namespace

ENABLE_BITMASK_OPERATORS(Ready);

class AnnotationController::Impl final
	: public Observable<IObserver>
	, public IDataProvider
	, IProgressController::IObserver
	, IJokeRequester::IClient
	, IFilterProvider::IObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(
		const std::shared_ptr<const ILogicFactory>&  logicFactory,
		std::shared_ptr<const ISettings>             settings,
		std::shared_ptr<const ICollectionProvider>   collectionProvider,
		std::shared_ptr<const IJokeRequesterFactory> jokeRequesterFactory,
		std::shared_ptr<const IDatabaseUser>         databaseUser,
		std::shared_ptr<IFilterProvider>             filterProvider
	)
		: m_logicFactory { logicFactory }
		, m_settings { std::move(settings) }
		, m_collectionProvider { std::move(collectionProvider) }
		, m_jokeRequesterFactory { std::move(jokeRequesterFactory) }
		, m_databaseUser { std::move(databaseUser) }
		, m_filterProvider { std::move(filterProvider) }
		, m_executor { logicFactory->GetExecutor() }
		, m_jokeRequesterClientImpl(JokeRequesterClientImpl::Create(*this))
	{
		m_jokeTimer.setSingleShot(true);
		m_jokeTimer.setInterval(std::chrono::seconds(1));
		QObject::connect(&m_jokeTimer, &QTimer::timeout, [this] {
			RequestJoke();
		});
		m_filterProvider->RegisterObserver(this);
	}

	~Impl() override
	{
		m_filterProvider->UnregisterObserver(this);
	}

public:
	void SetCurrentBookId(QString bookId, const bool extractNow)
	{
		if (auto parser = m_archiveParser.lock())
			parser->Stop();

		Perform(&IAnnotationController::IObserver::OnAnnotationRequested);
		if (m_currentBookId = std::move(bookId); !m_currentBookId.isEmpty())
			extractNow ? ExtractInfo() : m_extractInfoTimer->start();
		else if (!m_jokeRequesters.empty())
			m_jokeTimer.start();
	}

	void ShowJokes(const IJokeRequesterFactory::Implementation impl, const bool value) noexcept
	{
		if (value)
			m_jokeRequesters.emplace_back(impl, m_jokeRequesterFactory->Create(impl));
		else
			std::erase_if(m_jokeRequesters, [impl](const auto& item) {
				return item.first == impl;
			});
	}

	void ShowReviews(const bool value) noexcept
	{
		m_showReviews = value;
		if (!m_currentBookId.isEmpty())
			m_extractInfoTimer->start();
	}

private: // IDataProvider
	[[nodiscard]] const IDataItem& GetBook() const noexcept override
	{
		return *m_book;
	}

	[[nodiscard]] const IDataItem& GetSeries() const noexcept override
	{
		return *m_series;
	}

	[[nodiscard]] const IDataItem& GetAuthors() const noexcept override
	{
		return *m_authors;
	}

	[[nodiscard]] const IDataItem& GetGenres() const noexcept override
	{
		return *m_genres;
	}

	[[nodiscard]] const IDataItem& GetGroups() const noexcept override
	{
		return *m_groups;
	}

	[[nodiscard]] const IDataItem& GetKeywords() const noexcept override
	{
		return *m_keywords;
	}

	[[nodiscard]] const IDataItem& GetFolder() const noexcept override
	{
		return *m_folder;
	}

	[[nodiscard]] const IDataItem& GetUpdate() const noexcept override
	{
		return *m_update;
	}

	[[nodiscard]] const QString& GetError() const noexcept override
	{
		return m_archiveData.error;
	}

	[[nodiscard]] const QString& GetAnnotation() const noexcept override
	{
		return m_archiveData.annotation;
	}

	[[nodiscard]] const QString& GetEpigraph() const noexcept override
	{
		return m_archiveData.epigraph;
	}

	[[nodiscard]] const QString& GetEpigraphAuthor() const noexcept override
	{
		return m_archiveData.epigraphAuthor;
	}

	[[nodiscard]] const QString& GetLanguage() const noexcept override
	{
		return m_archiveData.language;
	}

	[[nodiscard]] const QString& GetSourceLanguage() const noexcept override
	{
		return m_archiveData.sourceLanguage;
	}

	[[nodiscard]] const std::vector<QString>& GetFb2Keywords() const noexcept override
	{
		return m_archiveData.keywords;
	}

	[[nodiscard]] const Covers& GetCovers() const noexcept override
	{
		return m_archiveData.covers;
	}

	[[nodiscard]] size_t GetTextSize() const noexcept override
	{
		return m_archiveData.textSize;
	}

	[[nodiscard]] size_t GetWordCount() const noexcept override
	{
		return m_archiveData.wordCount;
	}

	[[nodiscard]] IDataItem::Ptr GetContent() const noexcept override
	{
		return m_archiveData.content;
	}

	[[nodiscard]] IDataItem::Ptr GetDescription() const noexcept override
	{
		return m_archiveData.description;
	}

	[[nodiscard]] IDataItem::Ptr GetTranslators() const noexcept override
	{
		return m_archiveData.translators;
	}

	[[nodiscard]] const QString& GetPublisher() const noexcept override
	{
		return m_archiveData.publishInfo.publisher;
	}

	[[nodiscard]] const QString& GetPublishCity() const noexcept override
	{
		return m_archiveData.publishInfo.city;
	}

	[[nodiscard]] const QString& GetPublishYear() const noexcept override
	{
		return m_archiveData.publishInfo.year;
	}

	[[nodiscard]] const QString& GetPublishIsbn() const noexcept override
	{
		return m_archiveData.publishInfo.isbn;
	}

	[[nodiscard]] const ExportStatistics& GetExportStatistics() const noexcept override
	{
		return m_exportStatistics;
	}

	[[nodiscard]] const Reviews& GetReviews() const noexcept override
	{
		return m_reviews;
	}

	[[nodiscard]] std::vector<IDataItem::Flags> GetFlags(const NavigationMode navigationMode, const std::vector<QString>& ids) const override
	{
		return m_filterProvider->GetFlags(navigationMode, ids);
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

private: // IJokeRequester::IClient
	void OnTextReceived(const QString& value) override
	{
		Perform(&IAnnotationController::IObserver::OnJokeTextChanged, std::cref(value));
	}

	void OnImageReceived(const QByteArray& value) override
	{
		Perform(&IAnnotationController::IObserver::OnJokeImageChanged, std::cref(value));
	}

private: // IFilterProvider::IObserver
	void OnFilterEnabledChanged() override
	{
	}

	void OnFilterNavigationChanged(NavigationMode) override
	{
	}

	void OnFilterBooksChanged() override
	{
		Perform(&IAnnotationController::IObserver::OnAnnotationRequested);
	}

private:
	void ExtractInfo()
	{
		m_ready = Ready::None;
		auto db = m_databaseUser->Database();
		if (!db || m_currentBookId.isEmpty())
			return;

		m_databaseUser->Execute(
			{ "Get database book info",
		      [&, db = std::move(db), id = m_currentBookId.toLongLong()] {
				  return [this, book = CreateBook(*db, id)](size_t) mutable {
					  if (book->GetId() == m_currentBookId)
						  ExtractInfo(std::move(book));
				  };
			  } },
			3
		);
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
			m_book         = std::move(book);
			m_archiveData  = {};
			m_ready       |= Ready::Archive;
			return;
		}

		if (const auto progressController = m_archiveParserProgressController.lock())
			progressController->Stop();

		auto parser     = ILogicFactory::Lock(m_logicFactory)->CreateArchiveParser();
		m_archiveParser = parser;

		(*m_executor)({ "Get archive book info", [this, book = std::move(book), parser = std::move(parser)]() mutable {
						   const auto progressController = parser->GetProgressController();
						   progressController->RegisterObserver(this);
						   m_archiveParserProgressController = progressController;
						   auto data                         = parser->Parse(*book);
						   return [this, book = std::move(book), data = std::move(data)](size_t) mutable {
							   if (book->GetId() != m_currentBookId)
								   return;

							   m_archiveData  = std::move(data);
							   m_ready       |= Ready::Archive;

							   if (m_ready == Ready::All)
								   Perform(&IAnnotationController::IObserver::OnAnnotationChanged, std::cref(*this));
						   };
					   } });
	}

	void ExtractDatabaseInfo(IDataItem::Ptr book)
	{
		m_databaseUser->Execute(
			{ "Get database book additional info",
		      [this, book = std::move(book)]() mutable {
				  const auto db       = m_databaseUser->Database();
				  const auto bookId   = book->GetId().toLongLong();
				  auto       series   = CreateDictionary(*db, std::format(SERIES_QUERY, m_filterProvider->IsFilterEnabled() ? 1 : 0), bookId, &DatabaseUtil::CreateSeriesItem);
				  auto       authors  = CreateDictionary(*db, std::format(AUTHORS_QUERY, m_filterProvider->IsFilterEnabled() ? 1 : 0), bookId, &DatabaseUtil::CreateFullAuthorItem);
				  auto       genres   = CreateDictionary(*db, std::format(GENRES_QUERY, m_filterProvider->IsFilterEnabled() ? 1 : 0), bookId, &DatabaseUtil::CreateSimpleListItem);
				  auto       groups   = CreateDictionary(*db, GROUPS_QUERY, bookId, &DatabaseUtil::CreateSimpleListItem);
				  auto       keywords = CreateDictionary(*db, std::format(KEYWORDS_QUERY, m_filterProvider->IsFilterEnabled() ? 1 : 0), bookId, &DatabaseUtil::CreateSimpleListItem);
				  auto       folder   = CreateDictionary(*db, FOLDER_QUERY, bookId, &DatabaseUtil::CreateSimpleListItem);
				  auto       update   = CreateDictionary(*db, UPDATE_QUERY, bookId, &DatabaseUtil::CreateSimpleListItem);

				  ExportStatistics exportStatistics;
				  {
					  std::unordered_map<ExportStat::Type, std::vector<QDateTime>> exportStatisticsBuffer;
					  const auto                                                   query = db->CreateQuery("select ExportType, CreatedAt from Export_List_User where BookID = ?");
					  for (query->Bind(0, bookId), query->Execute(); !query->Eof(); query->Next())
						  exportStatisticsBuffer[static_cast<ExportStat::Type>(query->Get<int>(0))].emplace_back(QDateTime::fromString(query->Get<const char*>(1), Qt::ISODate));
					  std::ranges::move(exportStatisticsBuffer, std::back_inserter(exportStatistics));
				  }

				  return [this,
			              book             = std::move(book),
			              series           = std::move(series),
			              authors          = std::move(authors),
			              genres           = std::move(genres),
			              groups           = std::move(groups),
			              keywords         = std::move(keywords),
			              exportStatistics = std::move(exportStatistics),
			              folder           = std::move(folder),
			              update           = std::move(update),
			              reviews          = CollectReviews(*db, bookId)](size_t) mutable {
					  if (book->GetId() != m_currentBookId)
						  return;

					  m_book              = std::move(book);
					  m_series            = std::move(series);
					  m_authors           = std::move(authors);
					  m_genres            = std::move(genres);
					  m_groups            = std::move(groups);
					  m_keywords          = std::move(keywords);
					  m_exportStatistics  = std::move(exportStatistics);
					  m_folder            = std::move(folder);
					  m_update            = std::move(update);
					  m_reviews           = std::move(reviews);
					  m_ready            |= Ready::Database;

					  if (m_ready == Ready::All)
						  Perform(&IAnnotationController::IObserver::OnAnnotationChanged, std::cref(*this));
				  };
			  } },
			3
		);
	}

	Reviews CollectReviews(DB::IDatabase& db, const long long bookId) const
	{
		if (!m_showReviews)
			return {};

		std::vector<std::pair<QString, QString>> reviewFolders;
		{
			const auto query = db.CreateQuery(REVIEWS_QUERY);
			query->Bind(0, bookId);
			for (query->Execute(); !query->Eof(); query->Next())
				reviewFolders.emplace_back(query->Get<const char*>(0), query->Get<const char*>(1));
		}
		const auto archivesFolder = m_collectionProvider->GetActiveCollection().GetFolder() + "/" + QString::fromStdWString(REVIEWS_FOLDER);

		Reviews reviews;
		for (const auto& [libId, reviewFolder] : reviewFolders)
		{
			if (!QFile::exists(archivesFolder + "/" + reviewFolder))
				continue;

			Zip             zip(archivesFolder + "/" + reviewFolder);
			const auto      stream = zip.Read(libId);
			QJsonParseError jsonParseError;
			const auto      doc = QJsonDocument::fromJson(stream->GetStream().readAll(), &jsonParseError);
			if (jsonParseError.error != QJsonParseError::NoError)
			{
				PLOGW << jsonParseError.errorString();
				continue;
			}
			assert(doc.isArray());
			for (const auto jsonValue : doc.array())
			{
				assert(jsonValue.isObject());
				const auto obj    = jsonValue.toObject();
				auto&      review = reviews.emplace_back(QDateTime::fromString(obj[Constant::TIME].toString(), "yyyy-MM-dd hh:mm:ss"), obj[Constant::NAME].toString(), obj[Constant::TEXT].toString());
				if (review.name.isEmpty())
					review.name = Loc::Tr(Loc::Ctx::COMMON, Loc::ANONYMOUS);
			}
		}

		const auto reviewsSortMode = m_settings->Get("ui/View/AnnotationReviewSortMode", QString { "Time" }).toStdString();
		const auto invoker         = FindSecond(REVIEW_SORTERS, reviewsSortMode.data(), REVIEW_SORTER_DEFAULT, PszComparer {});
		std::invoke(invoker, std::ref(reviews));

		return reviews;
	}

	static IDataItem::Ptr CreateBook(DB::IDatabase& db, const long long id)
	{
		const auto query = db.CreateQuery(std::format(DatabaseUtil::BOOKS_QUERY, "", "", "from Books_View b", "where b.BookID = :id"));
		query->Bind(":id", id);
		query->Execute();
		return query->Eof() ? NavigationItem::Create() : DatabaseUtil::CreateBookItem(*query);
	}

	static IDataItem::Ptr CreateDictionary(DB::IDatabase& db, const std::string_view queryText, const long long id, const Extractor extractor)
	{
		auto       root  = NavigationItem::Create();
		const auto query = db.CreateQuery(queryText);
		query->Bind(":id", id);
		for (query->Execute(); !query->Eof(); query->Next())
		{
			auto child = extractor(*query);
			child->Reduce();
			root->AppendChild(std::move(child));
		}

		return root;
	}

	void RequestJoke()
	{
		if (m_jokeRequesters.empty())
			return;

		std::uniform_int_distribution<size_t> distribution(0, m_jokeRequesters.size() - 1);
		const auto                            index = distribution(m_mt);
		m_jokeRequesters[index].second->Request(m_jokeRequesterClientImpl);
	}

private:
	std::weak_ptr<const ILogicFactory>           m_logicFactory;
	std::shared_ptr<const ISettings>             m_settings;
	std::shared_ptr<const ICollectionProvider>   m_collectionProvider;
	std::shared_ptr<const IJokeRequesterFactory> m_jokeRequesterFactory;
	std::shared_ptr<const IDatabaseUser>         m_databaseUser;

	PropagateConstPtr<IFilterProvider, std::shared_ptr> m_filterProvider;

	std::vector<std::pair<IJokeRequesterFactory::Implementation, PropagateConstPtr<IJokeRequester, std::shared_ptr>>> m_jokeRequesters;

	PropagateConstPtr<Util::IExecutor>       m_executor;
	std::shared_ptr<IJokeRequester::IClient> m_jokeRequesterClientImpl;
	PropagateConstPtr<QTimer>                m_extractInfoTimer { Util::CreateUiTimer([&] {
        ExtractInfo();
    }) };

	QString m_currentBookId;

	Ready m_ready { Ready::None };

	std::weak_ptr<IProgressController> m_archiveParserProgressController;
	ArchiveParser::Data                m_archiveData;

	IDataItem::Ptr m_book;
	IDataItem::Ptr m_series;
	IDataItem::Ptr m_authors;
	IDataItem::Ptr m_genres;
	IDataItem::Ptr m_groups;
	IDataItem::Ptr m_keywords;
	IDataItem::Ptr m_folder;
	IDataItem::Ptr m_update;

	ExportStatistics m_exportStatistics;
	Reviews          m_reviews;

	std::weak_ptr<ArchiveParser> m_archiveParser;

	bool   m_showReviews { true };
	QTimer m_jokeTimer;

	std::random_device m_rd;
	std::mt19937       m_mt { m_rd() };
};

AnnotationController::AnnotationController(
	const std::shared_ptr<const ILogicFactory>&  logicFactory,
	std::shared_ptr<const ISettings>             settings,
	std::shared_ptr<const ICollectionProvider>   collectionProvider,
	std::shared_ptr<const IJokeRequesterFactory> jokeRequesterFactory,
	std::shared_ptr<const IDatabaseUser>         databaseUser,
	std::shared_ptr<IFilterProvider>       filterProvider
)
	: m_impl(logicFactory, std::move(settings), std::move(collectionProvider), std::move(jokeRequesterFactory), std::move(databaseUser), std::move(filterProvider))
{
	PLOGV << "AnnotationController created";
}

AnnotationController::~AnnotationController()
{
	PLOGV << "AnnotationController destroyed";
}

void AnnotationController::SetCurrentBookId(QString bookId, const bool extractNow)
{
	m_impl->SetCurrentBookId(std::move(bookId), extractNow);
}

QString AnnotationController::CreateAnnotation(const IDataProvider& dataProvider, const IStrategy& strategy) const
{
	const auto& book = dataProvider.GetBook();
	if (book.GetId().isEmpty())
		return {};

	QString annotation;
	strategy.Add(IStrategy::Section::Title, annotation, strategy.GenerateUrl(Constant::BOOK, book.GetId(), book.GetRawData(BookItem::Column::Title)), TITLE_PATTERN);
	strategy.Add(IStrategy::Section::Epigraph, annotation, dataProvider.GetEpigraph(), EPIGRAPH_PATTERN);
	strategy.Add(IStrategy::Section::EpigraphAuthor, annotation, dataProvider.GetEpigraphAuthor(), EPIGRAPH_PATTERN);
	strategy.Add(IStrategy::Section::Annotation, annotation, dataProvider.GetAnnotation());

	auto& keywords = dataProvider.GetKeywords();
	if (keywords.GetChildCount() == 0)
		strategy.Add(IStrategy::Section::Keywords, annotation, Join(dataProvider.GetFb2Keywords()), KEYWORDS_FB2);

	strategy.Add(IStrategy::Section::UrlTable, annotation, CreateUrlTable(dataProvider, strategy).ToString());

	if (const auto translators = dataProvider.GetTranslators(); translators && translators->GetChildCount() > 0)
	{
		QStringList translatorList;
		translatorList.reserve(static_cast<int>(translators->GetChildCount()));
		for (size_t i = 0, sz = translators->GetChildCount(); i < sz; ++i)
			translatorList << GetAuthorFull(*translators->GetChild(i));
		Table table(strategy);
		table.Add(TRANSLATORS, translatorList.join(", "));
		strategy.Add(IStrategy::Section::Translators, annotation, table.ToString());
	}

	{
		auto info = Table(strategy).Add(FILENAME, book.GetRawData(BookItem::Column::FileName));
		if (dataProvider.GetTextSize() > 0)
			info.Add(BOOK_SIZE, Tr(TEXT_SIZE).arg(dataProvider.GetTextSize()).arg(QChar(0x2248)).arg(std::max(1ULL, Round(dataProvider.GetTextSize() / 2000, -2))).arg(Round(dataProvider.GetWordCount(), -3)));
		info.Add(Loc::RATE, strategy.GenerateStars(book.GetRawData(BookItem::Column::LibRate).toInt()));
		info.Add(Loc::USER_RATE, strategy.GenerateStars(book.GetRawData(BookItem::Column::UserRate).toInt()));

		if (const auto& covers = dataProvider.GetCovers(); !covers.empty())
		{
			const auto total = std::accumulate(covers.cbegin(), covers.cend(), qsizetype { 0 }, [](const auto init, const auto& cover) {
				return init + cover.bytes.size();
			});
			info.Add(IMAGES, QString("%1 (%L2)").arg(covers.size()).arg(total));
		}

		strategy.Add(IStrategy::Section::BookInfo, annotation, info.ToString());
	}

	strategy.Add(IStrategy::Section::PublishInfo, annotation, GetPublishInfo(dataProvider));

	strategy.Add(IStrategy::Section::ParseError, annotation, dataProvider.GetError(), ERROR_PATTERN);

	if (!dataProvider.GetExportStatistics().empty())
	{
		const auto toDateList = [](const std::vector<QDateTime>& dates) {
			QStringList result;
			result.reserve(static_cast<int>(dates.size()));
			std::ranges::transform(dates, std::back_inserter(result), [](const QDateTime& date) {
				return date.toString("yy.MM.dd hh:mm");
			});
			return result.join(", ");
		};
		auto exportStatistics = Table(strategy).Add(EXPORT_STATISTICS, " ");
		for (const auto& [type, dates] : dataProvider.GetExportStatistics())
			exportStatistics.Add(GetName(type), toDateList(dates));
		strategy.Add(IStrategy::Section::ExportStatistics, annotation, exportStatistics.ToString());
	}

	if (!dataProvider.GetReviews().empty())
	{
		annotation.append(strategy.GetReviewsDelimiter()).append("<hr/>").append(QString(TITLE_PATTERN).arg(Tr(REVIEWS)));
		Table table(strategy);
		for (const auto& review : dataProvider.GetReviews())
			table.Add(QStringList() << review.name << review.time.toString("yyyy.MM.dd<br>hh:mm:ss") << review.text);
		strategy.Add(IStrategy::Section::Reviews, annotation, table.ToString());
	}

#ifndef NDEBUG
	PLOGV << annotation;
#endif
	return annotation;
}

void AnnotationController::ShowJokes(const IJokeRequesterFactory::Implementation impl, const bool value)
{
	m_impl->ShowJokes(impl, value);
}

void AnnotationController::ShowReviews(const bool value)
{
	m_impl->ShowReviews(value);
}

void AnnotationController::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void AnnotationController::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
