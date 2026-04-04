#include "BookInteractor.h"

#include "fnd/FindPair.h"

#include "database/interface/IDatabase.h"

#include "interface/constants/Enums.h"
#include "interface/constants/Localization.h"
#include "interface/constants/SettingsConstant.h"

#include "extract/BooksExtractor.h"

using namespace HomeCompa::Flibrary;

namespace
{

inline constexpr auto ON_BOOK_LINK_KEY             = "Preferences/Interaction/Book/OnLink";
inline constexpr auto ON_BOOK_DBL_CLICK_KEY        = "Preferences/Interaction/Book/OnDoubleClick";
inline constexpr auto ON_BOOK_RECENT_TRIGGERED_KEY = "Preferences/Interaction/Book/OnRecentSelect";

inline constexpr const char* NAVIGATION_TITLES[] = {
#define NAVIGATION_MODE_ITEM(NAME) #NAME,
	NAVIGATION_MODE_ITEMS_X_MACRO
#undef NAVIGATION_MODE_ITEM
};

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
};

static_assert(std::size(NAVIGATION_TITLES) == std::size(NAVIGATION_ID_QUERY));

class IBookInteractorImpl // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IBookInteractorImpl() = default;

public:
	virtual void Read(long long bookId) const         = 0;
	virtual void ExtractAsIs(long long bookId) const  = 0;
	virtual void ExtractAsZip(long long bookId) const = 0;
#define NAVIGATION_MODE_ITEM(NAME) virtual void FindWith##NAME(const long long) const = 0;
	NAVIGATION_MODE_ITEMS_X_MACRO
#undef NAVIGATION_MODE_ITEM
};

#define INTERACT_ITEMS_X_MACRO \
	INTERACT_ITEM(Read)        \
	INTERACT_ITEM(ExtractAsIs) \
	INTERACT_ITEM(ExtractAsZip)

constexpr std::pair<const char*, void (IBookInteractorImpl::*)(long long) const> INTERACT[] {
#define INTERACT_ITEM(NAME) { #NAME, &IBookInteractorImpl::NAME },
	INTERACT_ITEMS_X_MACRO
#undef INTERACT_ITEM
#define NAVIGATION_MODE_ITEM(NAME) {"FindWith"#NAME, &IBookInteractorImpl::FindWith##NAME},
		NAVIGATION_MODE_ITEMS_X_MACRO
#undef NAVIGATION_MODE_ITEM
};

} // namespace

struct BookInteractor::Impl final : IBookInteractorImpl
{
	std::weak_ptr<const ILogicFactory>         logicFactory;
	std::shared_ptr<const IUiFactory>          uiFactory;
	std::shared_ptr<const ISettings>           settings;
	std::shared_ptr<const IBookExtractor>      bookExtractor;
	std::shared_ptr<const IReaderController>   readerController;
	std::shared_ptr<const IDatabaseUser>       databaseUser;
	std::shared_ptr<const ICollectionProvider> collectionProvider;

	Impl(
		const std::shared_ptr<const ILogicFactory>& logicFactory,
		std::shared_ptr<const IUiFactory>           uiFactory,
		std::shared_ptr<const ISettings>            settings,
		std::shared_ptr<const IBookExtractor>       bookExtractor,
		std::shared_ptr<const IReaderController>    readerController,
		std::shared_ptr<const IDatabaseUser>        databaseUser,
		std::shared_ptr<const ICollectionProvider>  collectionProvider
	)
		: logicFactory { logicFactory }
		, uiFactory { std::move(uiFactory) }
		, settings { std::move(settings) }
		, bookExtractor { std::move(bookExtractor) }
		, readerController { std::move(readerController) }
		, databaseUser { std::move(databaseUser) }
		, collectionProvider { std::move(collectionProvider) }
	{
	}

	void Interact(const long long bookId, const QString& key) const
	{
		const auto invoker = FindSecond(INTERACT, settings->Get(key, QString {}).toStdString().data(), INTERACT[0].second, PszComparer {});
		std::invoke(invoker, this, bookId);
	}

private: // IBookInteractorImpl
	void Read(const long long bookId) const override
	{
		readerController->Read(bookId);
	}

	void ExtractAsIs(const long long bookId) const override
	{
		ExtractImpl(&BooksExtractor::ExtractAsIs, bookId);
	}

	void ExtractAsZip(const long long bookId) const override
	{
		ExtractImpl(&BooksExtractor::ExtractAsArchives, bookId);
	}

#define NAVIGATION_MODE_ITEM(NAME) void FindWith##NAME(const long long bookId) const override{ FindWithImpl(NavigationMode::NAME, bookId); }
	NAVIGATION_MODE_ITEMS_X_MACRO
#undef NAVIGATION_MODE_ITEM

private:
	void ExtractImpl(const BooksExtractor::Extract invoker, const long long bookId) const
	{
		auto book        = bookExtractor->GetExtractedBook(QString::number(bookId));
		book.dstFileName = settings->Get(Constant::Settings::EXPORT_TEMPLATE_KEY, IScriptController::GetDefaultOutputFileNameTemplate());

		const bool dstFolderRequired = IScriptController::HasMacro(book.dstFileName, IScriptController::Macro::UserDestinationFolder);
		auto       folder            = dstFolderRequired ? uiFactory->GetExistingDirectory(Constant::Settings::EXPORT_DIALOG_KEY, Loc::SELECT_SEND_TO_FOLDER) : QString {};

		const auto fillTemplateConverter = ILogicFactory::Lock(logicFactory)->CreateFillTemplateConverter();
		const auto db                    = databaseUser->Database();
		fillTemplateConverter->Fill(*db, book.dstFileName, book, folder);

		auto  extractor    = ILogicFactory::Lock(logicFactory)->CreateBooksExtractor();
		auto& extractorRef = *extractor;
		(extractorRef.*invoker)(folder, {}, { std::move(book) }, [extractor = std::move(extractor)](bool) mutable {
			extractor.reset();
		});
	}

	void FindWithImpl(const NavigationMode navigationMode, const long long bookId) const
	{
		const auto navigationIndex = static_cast<size_t>(navigationMode);
		if (!NAVIGATION_ID_QUERY[navigationIndex])
			return;

		const auto db    = databaseUser->Database();
		const auto query = db->CreateQuery(NAVIGATION_ID_QUERY[navigationIndex]);
		query->Bind(0, bookId);
		query->Execute();
		if (query->Eof())
			return;

		ILogicFactory::Lock(logicFactory)->FindBook(NAVIGATION_TITLES[navigationIndex], query->Get<QString>(0), bookId);
	}
};

BookInteractor::BookInteractor(
	const std::shared_ptr<const ILogicFactory>& logicFactory,
	std::shared_ptr<const IUiFactory>           uiFactory,
	std::shared_ptr<const ISettings>            settings,
	std::shared_ptr<const IBookExtractor>       bookExtractor,
	std::shared_ptr<const IReaderController>    readerController,
	std::shared_ptr<const IDatabaseUser>        databaseUser,
	std::shared_ptr<const ICollectionProvider>  collectionProvider
)
	: m_impl { logicFactory, std::move(uiFactory), std::move(settings), std::move(bookExtractor), std::move(readerController), std::move(databaseUser), std::move(collectionProvider) }
{
}

BookInteractor::~BookInteractor() = default;

void BookInteractor::OnLinkActivated(const long long bookId) const
{
	m_impl->Interact(bookId, ON_BOOK_LINK_KEY);
}

void BookInteractor::OnDoubleClicked(const long long bookId) const
{
	m_impl->Interact(bookId, ON_BOOK_DBL_CLICK_KEY);
}

void BookInteractor::OnRecentBookMenuTriggered(const long long bookId) const
{
	m_impl->Interact(bookId, ON_BOOK_RECENT_TRIGGERED_KEY);
}
