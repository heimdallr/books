#include "BookInteractor.h"

#include "fnd/FindPair.h"

#include "interface/constants/Localization.h"
#include "interface/constants/SettingsConstant.h"

#include "extract/BooksExtractor.h"

using namespace HomeCompa::Flibrary;

namespace
{
constexpr auto ON_BOOK_LINK_KEY      = "ui/Interaction/Book/OnLink";
constexpr auto ON_BOOK_DBL_CLICK_KEY = "ui/Interaction/Book/OnDoubleClick";

class IBookInteractorImpl // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IBookInteractorImpl() = default;

public:
	virtual void Read(long long bookId) const              = 0;
	virtual void ExtractAsFb2(long long bookId) const       = 0;
	virtual void ExtractAsZip(long long bookId) const = 0;
};

#define INTERACT_ITEMS_X_MACRO       \
	INTERACT_ITEM(Read)              \
	INTERACT_ITEM(ExtractAsFb2) \
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

	Impl(
		const std::shared_ptr<const ILogicFactory>& logicFactory,
		std::shared_ptr<const IUiFactory>           uiFactory,
		std::shared_ptr<const ISettings>            settings,
		std::shared_ptr<const IBookExtractor>       bookExtractor,
		std::shared_ptr<const IReaderController>    readerController
	)
		: logicFactory { logicFactory }
		, uiFactory { std::move(uiFactory) }
		, settings { std::move(settings) }
		, bookExtractor { std::move(bookExtractor) }
		, readerController { std::move(readerController) }
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

	void ExtractAsFb2(const long long bookId) const override
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
		auto       outputFileNameTemplate = settings->Get(Constant::Settings::EXPORT_TEMPLATE_KEY, IScriptController::GetDefaultOutputFileNameTemplate());
		const bool dstFolderRequired      = IScriptController::HasMacro(outputFileNameTemplate, IScriptController::Macro::UserDestinationFolder);
		auto       folder                 = dstFolderRequired ? uiFactory->GetExistingDirectory(Constant::Settings::EXPORT_DIALOG_KEY, Loc::SELECT_SEND_TO_FOLDER) : QString {};
		auto       book                   = bookExtractor->GetExtractedBook(QString::number(bookId));

		auto  extractor    = ILogicFactory::Lock(logicFactory)->CreateBooksExtractor();
		auto& extractorRef = *extractor;
		(extractorRef.*invoker)(std::move(folder), {}, { std::move(book) }, std::move(outputFileNameTemplate), [extractor = std::move(extractor)](bool) mutable {
			extractor.reset();
		});
	}
};

BookInteractor::BookInteractor(
	const std::shared_ptr<const ILogicFactory>& logicFactory,
	std::shared_ptr<const IUiFactory>           uiFactory,
	std::shared_ptr<const ISettings>            settings,
	std::shared_ptr<const IBookExtractor>       bookExtractor,
	std::shared_ptr<const IReaderController>    readerController
)
	: m_impl { logicFactory, std::move(uiFactory), std::move(settings), std::move(bookExtractor), std::move(readerController) }
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
