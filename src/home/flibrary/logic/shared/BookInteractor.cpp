#include "BookInteractor.h"

#include <QTemporaryDir>

#include "fnd/FindPair.h"

#include "database/interface/IDatabase.h"

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/constants/SettingsConstant.h"
#include "interface/localization.h"

#include "data/DataItem.h"
#include "extract/BooksExtractor.h"
#include "util/GenresLocalization.h"
#include "util/StrUtil.h"

#include "QtTypes.h"
#include "log.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace {

inline constexpr auto ON_BOOK_LINK_KEY             = "Preferences/Interaction/Book/OnLink";
inline constexpr auto ON_BOOK_DBL_CLICK_KEY        = "Preferences/Interaction/Book/OnDoubleClick";
inline constexpr auto ON_BOOK_RECENT_TRIGGERED_KEY = "Preferences/Interaction/Book/OnRecentSelect";

inline constexpr const char* NAVIGATION_ID_QUERY[] = {
	"select AuthorID from Author_List where BookID = ? order by OrdNum limit 1",
	"select SeriesID from Series_List where BookID = ? order by OrdNum limit 1",
	"select GenreCode from Genre_List where BookID = ? order by OrdNum limit 1",
	"select Year from Books where BookID = ?",
	"select KeywordID from Keyword_List where BookID = ? order by OrdNum limit 1",
	"select UpdateID from Books where BookID = ?",
	"select FolderID from Books where BookID = ?",
	"select Lang from Books where BookID = ?",
	"select GroupID from Groups_List_User where ObjectID = ? order by CreatedAt desc limit 1",
	nullptr,
	nullptr,
	"select 42",
	"select 42",
	"select 42",
};

static_assert(std::size(NAVIGATION_ID_QUERY) == static_cast<size_t>(NavigationMode::Last));

#define INTERACT_ITEMS_X_MACRO     \
	INTERACT_ITEM(Read)            \
	INTERACT_ITEM(ExtractAsIs)     \
	INTERACT_ITEM(ExtractAsZip)    \
	INTERACT_ITEM(Script)          \
	INTERACT_ITEM(NavigationLink)

class IBookInteractorImpl // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IBookInteractorImpl() = default;

public:
#define INTERACT_ITEM(NAME) virtual void NAME(long long, const QStringList &, const QString&) const = 0;
	INTERACT_ITEMS_X_MACRO
#undef INTERACT_ITEM
#define NAVIGATION_MODE_ITEM(NAME) virtual void FindWith##NAME(long long, const QStringList &, const QString&) const = 0;
	NAVIGATION_MODE_ITEMS_X_MACRO
#undef NAVIGATION_MODE_ITEM
};

constexpr std::pair<const char*, void (IBookInteractorImpl::*)(long long, const QStringList&, const QString&) const> INTERACT[] {
#define INTERACT_ITEM(NAME) { #NAME, &IBookInteractorImpl::NAME },
	INTERACT_ITEMS_X_MACRO
#undef INTERACT_ITEM
#define NAVIGATION_MODE_ITEM(NAME) { "FindWith" #NAME, &IBookInteractorImpl::FindWith##NAME },
		NAVIGATION_MODE_ITEMS_X_MACRO
#undef NAVIGATION_MODE_ITEM
};

struct ExtractSettings
{
	QString parameter;
	QString dstFileName;
	QString ext;
	bool    dstFolderRequired { true };
	bool    createFillTemplateConverterParameter { false };
	bool    tempFolder { false };
};

QString GetAuthorFull(const auto& tuple)
{
	QString result = std::get<1>(tuple);
	Util::AppendTitle(result, std::get<2>(tuple));
	Util::AppendTitle(result, std::get<3>(tuple));
	return result;
}

QString GetAuthorReduced(const auto& tuple)
{
	QString last   = std::get<1>(tuple);
	QString first  = std::get<2>(tuple);
	QString middle = std::get<3>(tuple);

	for (int i = 0; i < 2; ++i)
	{
		if (!last.isEmpty())
			break;

		last   = std::move(first);
		first  = std::move(middle);
		middle = QString {};
	}

	if (last.isEmpty())
		last = Loc::Tr(Loc::Ctx::ERROR_CTX, Loc::AUTHOR_NOT_SPECIFIED);

	auto name = last;

	const auto append = [&](const QString& str) {
		if (str.isEmpty())
			return;

		Util::AppendTitle(name, First(str, 1) + ".");
	};

	append(first);
	append(middle);

	return name;
}

std::pair<QString, QString> MatchAlways(const QStringView, const DB::IQuery& query)
{
	return query.Get<QString, QString>(0, 1);
}

std::pair<QString, QString> MatchAuthor(const QStringView title, const DB::IQuery& query)
{
	auto author = query.Get<QString, const char*, const char*, const char*>();
	if (auto name = GetAuthorFull(author); title.contains(name, Qt::CaseInsensitive) || title.contains(GetAuthorReduced(author), Qt::CaseInsensitive))
		return std::make_pair(std::move(std::get<0>(author)), std::move(name));
	return {};
}

std::pair<QString, QString> MatchGenre(const QStringView title, const DB::IQuery& query)
{
	auto genre = query.Get<QString, const char*, const char*>();
	if (QString name = std::get<2>(genre); !name.isEmpty() && title.contains(name, Qt::CaseInsensitive))
		return std::make_pair(std::move(std::get<0>(genre)), std::move(name));

	if (auto name = Loc::Tr(Util::GENRE, std::get<1>(genre)); title.contains(name, Qt::CaseInsensitive))
		return std::make_pair(std::move(std::get<0>(genre)), std::move(name));

	return {};
}

} // namespace

struct BookInteractor::Impl final : IBookInteractorImpl
{
	std::weak_ptr<const ILogicFactory>         logicFactory;
	std::shared_ptr<const IUiFactory>          uiFactory;
	std::shared_ptr<const ISettings>           settings;
	std::shared_ptr<const IBookExtractor>      bookExtractor;
	std::shared_ptr<const IReaderController>   readerController;
	std::shared_ptr<const IDatabaseProvider>   databaseUser;
	std::shared_ptr<const ICollectionProvider> collectionProvider;
	std::shared_ptr<IScriptControllerProvider> scriptControllerProvider;

	Impl(
		const std::shared_ptr<const ILogicFactory>& logicFactory,
		std::shared_ptr<const IUiFactory>           uiFactory,
		std::shared_ptr<const ISettings>            settings,
		std::shared_ptr<const IBookExtractor>       bookExtractor,
		std::shared_ptr<const IReaderController>    readerController,
		std::shared_ptr<const IDatabaseProvider>    databaseUser,
		std::shared_ptr<const ICollectionProvider>  collectionProvider,
		std::shared_ptr<IScriptControllerProvider>  scriptControllerProvider
	)
		: logicFactory { logicFactory }
		, uiFactory { std::move(uiFactory) }
		, settings { std::move(settings) }
		, bookExtractor { std::move(bookExtractor) }
		, readerController { std::move(readerController) }
		, databaseUser { std::move(databaseUser) }
		, collectionProvider { std::move(collectionProvider) }
		, scriptControllerProvider { std::move(scriptControllerProvider) }
	{
	}

	void Interact(const long long bookId, const QString& key, const QString& parameters) const
	{
		auto list = settings->Get(key, QString {}).split('.');
		assert(!list.isEmpty());
		const auto invoker = FindSecond(INTERACT, list.front().toStdString().data(), INTERACT[0].second, PszComparer {});
		list.pop_front();
		std::invoke(invoker, this, bookId, list, parameters);
	}

private: // IBookInteractorImpl
	void Read(const long long bookId, const QStringList&, const QString&) const override
	{
		readerController->Read(bookId);
	}

	void ExtractAsIs(const long long bookId, const QStringList&, const QString&) const override
	{
		ExtractImpl(&BooksExtractor::ExtractAsIs, bookId);
	}

	void ExtractAsZip(const long long bookId, const QStringList&, const QString&) const override
	{
		ExtractImpl(&BooksExtractor::ExtractAsArchives, bookId, { .ext = "zip" });
	}

	void Script(const long long bookId, const QStringList& parameters, const QString&) const override
	{
		assert(!parameters.isEmpty());
		const auto  scriptController = scriptControllerProvider->GetScriptController();
		const auto& scripts          = scriptController->GetScripts();
		if (const auto it = std::ranges::find(
				scripts,
				parameters.front(),
				[](const auto& item) {
					return item.name;
				}
			);
		    it != scripts.end())
			return ExtractImpl(
				&BooksExtractor::ExtractAsScript,
				bookId,
				{
					.parameter   = it->uid,
					.dstFileName = QString("%1/%2").arg(IScriptController::GetMacro(IScriptController::Macro::UserDestinationFolder)).arg(IScriptController::GetMacro(IScriptController::Macro::FileName)),
					.dstFolderRequired                    = false,
					.createFillTemplateConverterParameter = true,
					.tempFolder                           = true,
				}
			);

		PLOGW << "Script " << parameters.front() << " not found";
	}

	void NavigationLink(const long long bookId, const QStringList&, const QString& parameters) const override
	{
		using Matcher     = std::pair<QString, QString> (*)(QStringView, const DB::IQuery&);
		using Description = std::tuple<NavigationMode, const char*, Matcher>;

		static constexpr Description defaultDescription = { NavigationMode::Unknown, "", nullptr };
		static constexpr auto        authorQueryText    = "select a.AuthorID, a.LastName, a.FirstName, a.MiddleName from Authors a join Author_List l on l.AuthorID = a.AuthorID and l.BookID = ?";

		static constexpr std::pair<int, Description> descriptions[] {
			{     BookItem::Column::Author,                                                                                                           { NavigationMode::Authors, authorQueryText, &MatchAuthor } },
			{ BookItem::Column::AuthorFull,                                                                                                           { NavigationMode::Authors, authorQueryText, &MatchAuthor } },
			{     BookItem::Column::Series,            { NavigationMode::Series, "select s.SeriesID, s.SeriesTitle from Series s join Series_List l on l.SeriesID = s.SeriesID and l.BookID = ?", &MatchAlways } },
			{     BookItem::Column::Folder,               { NavigationMode::Archives, "select f.FolderID, f.FolderTitle from Folders f join Books b on b.FolderID = f.FolderID and b.BookID = ?", &MatchAlways } },
			{       BookItem::Column::Lang,                                                                 { NavigationMode::Languages, "select b.Lang, b.Lang from Books b where b.BookID = ?", &MatchAlways } },
			{       BookItem::Column::Year,                                                               { NavigationMode::PublishYear, "select b.Year, b.Year from Books b where b.BookID = ?", &MatchAlways } },
			{ BookItem::Column::UpdateDate,                                                         { NavigationMode::Updates, "select b.UpdateID, b.UpdateDate from Books b where b.BookID = ?", &MatchAlways } },
			{      BookItem::Column::Genre, { NavigationMode::Genres, "select g.GenreCode, g.FB2Code, g.GenreTitle from Genres g join Genre_List l on l.GenreCode = g.GenreCode and l.BookID = ?", &MatchGenre } },
		};

		if (parameters.isEmpty())
			return Read(bookId, {}, {});

		const auto [id, title] = [&]() -> std::pair<int, QStringView> {
			const auto index = parameters.indexOf('|');
			if (index < 0)
				return {};

			return std::make_pair(QStringView(parameters.begin(), std::next(parameters.begin(), index)).toInt(), QStringView(std::next(parameters.begin(), index + 1), parameters.end()));
		}();

		if (title.isEmpty())
			return Read(bookId, {}, {});

		const auto& description = FindSecond(descriptions, id, defaultDescription);
		if (std::get<0>(description) == NavigationMode::Unknown)
			return Read(bookId, {}, {});

		const auto db    = databaseUser->GetDatabase();
		const auto query = db->CreateQuery(std::get<1>(description), bookId);
		const auto match = *query | std::views::transform([&](const auto& item) {
			return std::invoke(std::get<2>(description), title, item);
		}) | std::views::filter([](const auto& item) {
			return !item.first.isEmpty();
		}) | std::ranges::to<std::vector>();

		if (match.empty())
			return assert(false && "unexpected logic"), Read(bookId, {}, {});

		if (match.size() == 1)
			return ILogicFactory::Lock(logicFactory)->FindBook(std::get<0>(description), match.front().first);

		uiFactory->ExecMenu(match | std::views::values | std::ranges::to<QStringList>(), [&](const int n) {
			ILogicFactory::Lock(logicFactory)->FindBook(std::get<0>(description), match[static_cast<size_t>(n)].first);
		});
	}

#define NAVIGATION_MODE_ITEM(NAME) void FindWith##NAME(const long long bookId, const QStringList&, const QString&) const override { FindWithImpl(NavigationMode::NAME, bookId); }
	NAVIGATION_MODE_ITEMS_X_MACRO
#undef NAVIGATION_MODE_ITEM

private:
	void ExtractImpl(const BooksExtractor::Extract invoker, const long long bookId, const ExtractSettings& options = {}) const
	{
		auto book        = bookExtractor->GetExtractedBook(QString::number(bookId));
		book.dstFileName = options.dstFileName.isEmpty() ? settings->Get(Constant::Settings::EXPORT_TEMPLATE_KEY, IScriptController::GetDefaultOutputFileNameTemplate()) : options.dstFileName;

		const bool                     dstFolderRequired = options.dstFolderRequired && IScriptController::HasMacro(book.dstFileName, IScriptController::Macro::UserDestinationFolder);
		auto                           folder            = dstFolderRequired ? uiFactory->GetExistingDirectory(Constant::Settings::EXPORT_DIALOG_KEY, Loc::SELECT_SEND_TO_FOLDER) : QString {};
		std::shared_ptr<QTemporaryDir> tempDir;
		if (options.tempFolder)
			tempDir = std::make_shared<QTemporaryDir>();

		const auto fillTemplateConverter = ILogicFactory::Lock(logicFactory)->CreateFillTemplateConverter(options.createFillTemplateConverterParameter);
		const auto db                    = databaseUser->GetDatabase();
		fillTemplateConverter->Fill(*db, book.dstFileName, book, tempDir ? tempDir->filePath("") : folder);

		if (!options.ext.isEmpty())
		{
			const QFileInfo fileInfo(book.dstFileName);
			book.dstFileName = fileInfo.dir().filePath(fileInfo.completeBaseName() + "." + options.ext);
		}

		auto  extractor    = ILogicFactory::Lock(logicFactory)->CreateBooksExtractor();
		auto& extractorRef = *extractor;
		(extractorRef.*invoker)(folder, options.parameter, { std::move(book) }, [extractor = std::move(extractor)](bool) mutable {
			extractor.reset();
		});
	}

	void FindWithImpl(const NavigationMode navigationMode, const long long bookId) const
	{
		const auto navigationIndex = static_cast<size_t>(navigationMode);
		if (!NAVIGATION_ID_QUERY[navigationIndex])
			return;

		const auto db    = databaseUser->GetDatabase();
		const auto query = db->CreateQuery(NAVIGATION_ID_QUERY[navigationIndex]);
		query->Execute(bookId);
		if (!query->Eof())
			ILogicFactory::Lock(logicFactory)->FindBook(navigationMode, query->Get<QString>(0), bookId);
	}
};

BookInteractor::BookInteractor(
	const std::shared_ptr<const ILogicFactory>& logicFactory,
	std::shared_ptr<const IUiFactory>           uiFactory,
	std::shared_ptr<const ISettings>            settings,
	std::shared_ptr<const IBookExtractor>       bookExtractor,
	std::shared_ptr<const IReaderController>    readerController,
	std::shared_ptr<const IDatabaseProvider>    databaseUser,
	std::shared_ptr<const ICollectionProvider>  collectionProvider,
	std::shared_ptr<IScriptControllerProvider>  scriptControllerProvider
)
	: m_impl { logicFactory,
	           std::move(uiFactory),
	           std::move(settings),
	           std::move(bookExtractor),
	           std::move(readerController),
	           std::move(databaseUser),
	           std::move(collectionProvider),
	           std::move(scriptControllerProvider) }
{
}

BookInteractor::~BookInteractor() = default;

void BookInteractor::OnLinkActivated(const long long bookId, const QString& parameters) const
{
	m_impl->Interact(bookId, ON_BOOK_LINK_KEY, parameters);
}

void BookInteractor::OnDoubleClicked(const long long bookId, const QString& parameters) const
{
	m_impl->Interact(bookId, ON_BOOK_DBL_CLICK_KEY, parameters);
}

void BookInteractor::OnRecentBookMenuTriggered(const long long bookId, const QString& parameters) const
{
	m_impl->Interact(bookId, ON_BOOK_RECENT_TRIGGERED_KEY, parameters);
}
