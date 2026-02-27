#include "BookInteractor.h"

#include "fnd/FindPair.h"

#include "interface/constants/Localization.h"
#include "interface/constants/SettingsConstant.h"

#include "extract/BooksExtractor.h"

using namespace HomeCompa::Flibrary;

namespace
{

constexpr auto ON_BOOK_LINK_KEY      = "Preferences/Interaction/Book/OnLink";
constexpr auto ON_BOOK_DBL_CLICK_KEY = "Preferences/Interaction/Book/OnDoubleClick";

class IBookInteractorImpl // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IBookInteractorImpl() = default;

public:
	virtual void Read(long long bookId) const         = 0;
	virtual void ExtractAsIs(long long bookId) const  = 0;
	virtual void ExtractAsZip(long long bookId) const = 0;
};

#define INTERACT_ITEMS_X_MACRO \
	INTERACT_ITEM(Read)        \
	INTERACT_ITEM(ExtractAsIs) \
	INTERACT_ITEM(ExtractAsZip)

constexpr std::pair<const char*, void (IBookInteractorImpl::*)(long long) const> INTERACT[] {
#define INTERACT_ITEM(NAME) { #NAME, &IBookInteractorImpl::NAME },
	INTERACT_ITEMS_X_MACRO
#undef INTERACT_ITEM
};

} // namespace

struct BookInteractor::Impl final : IBookInteractorImpl
{
	std::weak_ptr<const ILogicFactory>       logicFactory;
	std::shared_ptr<const IUiFactory>        uiFactory;
	std::shared_ptr<const ISettings>         settings;
	std::shared_ptr<const IBookExtractor>    bookExtractor;
	std::shared_ptr<const IReaderController> readerController;
	std::shared_ptr<const IDatabaseUser>     databaseUser;

	Impl(
		const std::shared_ptr<const ILogicFactory>& logicFactory,
		std::shared_ptr<const IUiFactory>           uiFactory,
		std::shared_ptr<const ISettings>            settings,
		std::shared_ptr<const IBookExtractor>       bookExtractor,
		std::shared_ptr<const IReaderController>    readerController,
		std::shared_ptr<const IDatabaseUser>        databaseUser
	)
		: logicFactory { logicFactory }
		, uiFactory { std::move(uiFactory) }
		, settings { std::move(settings) }
		, bookExtractor { std::move(bookExtractor) }
		, readerController { std::move(readerController) }
		, databaseUser { std::move(databaseUser) }
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
};

BookInteractor::BookInteractor(
	const std::shared_ptr<const ILogicFactory>& logicFactory,
	std::shared_ptr<const IUiFactory>           uiFactory,
	std::shared_ptr<const ISettings>            settings,
	std::shared_ptr<const IBookExtractor>       bookExtractor,
	std::shared_ptr<const IReaderController>    readerController,
	std::shared_ptr<const IDatabaseUser>        databaseUser
)
	: m_impl { logicFactory, std::move(uiFactory), std::move(settings), std::move(bookExtractor), std::move(readerController), std::move(databaseUser) }
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
