#include "BookExtractor.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/constants/SettingsConstant.h"
#include "interface/logic/IScriptController.h"

#include "util/DyLib.h"
#include "util/translit.h"

#include "log.h"
#include "data/DataItem.h"

using namespace HomeCompa;
using namespace Flibrary;

namespace
{

constexpr auto OPDS_TRANSLITERATE = "opds/transliterate";

QString GetOutputFileNameTemplate(const ISettings& settings)
{
	auto outputFileNameTemplate = settings.Get(Constant::Settings::EXPORT_TEMPLATE_KEY, IScriptController::GetDefaultOutputFileNameTemplate());
	IScriptController::SetMacro(outputFileNameTemplate, IScriptController::Macro::UserDestinationFolder, "");
	return outputFileNameTemplate;
}

} // namespace

struct BookExtractor::Impl
{
	std::shared_ptr<const ISettings>           settings;
	std::shared_ptr<const ICollectionProvider> collectionProvider;
	std::shared_ptr<const IDatabaseUser>       databaseUser;
	const QString                              m_outputFileNameTemplate { GetOutputFileNameTemplate(*settings) };

	Impl(std::shared_ptr<const ISettings> settings, std::shared_ptr<const ICollectionProvider> collectionProvider, std::shared_ptr<const IDatabaseUser> databaseUser)
		: settings { std::move(settings) }
		, collectionProvider { std::move(collectionProvider) }
		, databaseUser { std::move(databaseUser) }
	{
	}

	ILogicFactory::ExtractedBook GetExtractedBook(const QString& bookId) const
	{
		const auto db    = databaseUser->Database();
		const auto query = db->CreateQuery(R"(
select
    f.FolderTitle,
    b.FileName||b.Ext,
    b.BookSize,
    s.SeriesTitle,
    b.SeqNumber,
    b.Title,
    a.FirstName, a.MiddleName, a.LastName
from Books b
join Folders f on f.FolderID = b.FolderID
join Author_List al on al.BookID = b.BookID
join Authors a on a.AuthorID = al.AuthorID
left join Series s on s.SeriesID = b.SeriesID
where b.BookID = 1
order by al.OrdNum limit 1
)");
		query->Bind(0, bookId.toLongLong());
		query->Execute();
		if (query->Eof())
			return {};

		auto result = ILogicFactory::ExtractedBook {
			.id        = bookId.toLongLong(),
			.folder    = query->Get<const char*>(0),
			.file      = query->Get<const char*>(1),
			.size      = query->Get<long long>(2),
			.series    = query->Get<const char*>(3),
			.seqNumber = query->Get<int>(4),
			.title     = query->Get<const char*>(5),
			.authorFull = { .firstName = query->Get<const char*>(6), .middleName = query->Get<const char*>(7), .lastName = query->Get<const char*>(8), },
		};

		result.author = result.authorFull.lastName;
		AppendTitle(result.author, result.authorFull.firstName);
		AppendTitle(result.author, result.authorFull.middleName);

		return result;
	}

	ILogicFactory::ExtractedBook::Author GetExtractedBookAuthor(const QString& bookId) const
	{
		const auto db    = databaseUser->Database();
		const auto query = db->CreateQuery(R"(
select a.FirstName, a.MiddleName, a.LastName
from Authors a
join Author_List al on al.AuthorID = a.AuthorID and al.BookID = ?
order by al.OrdNum limit 1
)");
		query->Bind(0, bookId.toLongLong());
		query->Execute();
		if (query->Eof())
			return {};

		return {
			.firstName  = query->Get<const char*>(0),
			.middleName = query->Get<const char*>(1),
			.lastName   = query->Get<const char*>(2),
		};
	}

	QString GetFileName(const ILogicFactory::ExtractedBook& book) const
	{
		auto outputFileName = m_outputFileNameTemplate;
		auto db             = databaseUser->Database();
		ILogicFactory::FillScriptTemplate(*db, outputFileName, book);
		if (!settings->Get(OPDS_TRANSLITERATE, false))
			return outputFileName;

		LoadICU();
		return Util::Transliterate(m_icuTransliterate, outputFileName);
	}

private:
	void LoadICU() const
	{
		if (m_icuTransliterate)
			return;

		if (!((m_icuLib = std::make_unique<Util::DyLib>(ICU::LIB_NAME))))
		{
			PLOGW << "Cannot load " << ICU::LIB_NAME << " dynamic library";
			return;
		}

		if (!((m_icuTransliterate = m_icuLib->GetTypedProc<ICU::TransliterateType>(ICU::TRANSLITERATE_NAME))))
			PLOGW << "Cannot find entry point " << ICU::TRANSLITERATE_NAME << " in " << ICU::LIB_NAME << " dynamic library";
	}

	mutable std::unique_ptr<Util::DyLib> m_icuLib;
	mutable ICU::TransliterateType       m_icuTransliterate { nullptr };
};

BookExtractor::BookExtractor(std::shared_ptr<const ISettings> settings, std::shared_ptr<const ICollectionProvider> collectionProvider, std::shared_ptr<const IDatabaseUser> databaseUser)
	: m_impl { std::move(settings), std::move(collectionProvider), std::move(databaseUser) }
{
}

BookExtractor::~BookExtractor() = default;

QString BookExtractor::GetFileName(const QString& bookId) const
{
	const auto book = GetExtractedBook(bookId);
	return GetFileName(book);
}

QString BookExtractor::GetFileName(const ILogicFactory::ExtractedBook& book) const
{
	return m_impl->GetFileName(book);
}

ILogicFactory::ExtractedBook BookExtractor::GetExtractedBook(const QString& bookId) const
{
	return m_impl->GetExtractedBook(bookId);
}

ILogicFactory::ExtractedBook::Author BookExtractor::GetExtractedBookAuthor(const QString& bookId) const
{
	return m_impl->GetExtractedBookAuthor(bookId);
}
