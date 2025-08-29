#include "BookExtractor.h"

#include "database/interface/IDatabase.h"
#include "database/interface/IQuery.h"

#include "interface/constants/SettingsConstant.h"
#include "interface/logic/IScriptController.h"

#include "util/DyLib.h"
#include "util/translit.h"

#include "log.h"

using namespace HomeCompa;
using namespace Opds;

namespace
{

constexpr auto OPDS_TRANSLITERATE = "opds/transliterate";

QString GetOutputFileNameTemplate(const ISettings& settings)
{
	auto outputFileNameTemplate = settings.Get(Flibrary::Constant::Settings::EXPORT_TEMPLATE_KEY, Flibrary::IScriptController::GetDefaultOutputFileNameTemplate());
	Flibrary::IScriptController::SetMacro(outputFileNameTemplate, Flibrary::IScriptController::Macro::UserDestinationFolder, "");
	return outputFileNameTemplate;
}

} // namespace

struct BookExtractor::Impl
{
	std::shared_ptr<const ISettings> settings;
	std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider;
	std::shared_ptr<const Flibrary::IDatabaseController> databaseController;
	const QString m_outputFileNameTemplate { GetOutputFileNameTemplate(*settings) };

	Impl(std::shared_ptr<const ISettings> settings, std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider, std::shared_ptr<const Flibrary::IDatabaseController> databaseController)
		: settings { std::move(settings) }
		, collectionProvider { std::move(collectionProvider) }
		, databaseController { std::move(databaseController) }
	{
	}

	Flibrary::ILogicFactory::ExtractedBook GetExtractedBook(const QString& bookId) const
	{
		const auto db = databaseController->GetDatabase(true);
		const auto query = db->CreateQuery(R"(
select
    f.FolderTitle,
    b.FileName||b.Ext,
    b.BookSize,
    (select a.LastName ||' '||a.FirstName ||' '||a.MiddleName from Authors a join Author_List al on al.AuthorID = a.AuthorID and al.BookID = b.BookID order by al.AuthorID limit 1),
    s.SeriesTitle,
    b.SeqNumber,
    b.Title
from Books b
join Folders f on f.FolderID = b.FolderID
left join Series s on s.SeriesID = b.SeriesID
where b.BookID = ?
)");
		query->Bind(0, bookId.toLongLong());
		query->Execute();
		if (query->Eof())
			return {};

		return Flibrary::ILogicFactory::ExtractedBook { bookId.toInt(),           query->Get<const char*>(0), query->Get<const char*>(1),
			                                            query->Get<long long>(2), query->Get<const char*>(3), query->Get<const char*>(4),
			                                            query->Get<int>(5),       query->Get<const char*>(6) };
	}

	QString GetFileName(const Flibrary::ILogicFactory::ExtractedBook& book) const
	{
		auto outputFileName = m_outputFileNameTemplate;
		Flibrary::ILogicFactory::FillScriptTemplate(outputFileName, book);
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
	mutable ICU::TransliterateType m_icuTransliterate { nullptr };
};

BookExtractor::BookExtractor(std::shared_ptr<const ISettings> settings,
                             std::shared_ptr<const Flibrary::ICollectionProvider> collectionProvider,
                             std::shared_ptr<const Flibrary::IDatabaseController> databaseController)
	: m_impl { std::move(settings), std::move(collectionProvider), std::move(databaseController) }
{
}

BookExtractor::~BookExtractor() = default;

QString BookExtractor::GetFileName(const QString& bookId) const
{
	const auto book = GetExtractedBook(bookId);
	return GetFileName(book);
}

QString BookExtractor::GetFileName(const Flibrary::ILogicFactory::ExtractedBook& book) const
{
	return m_impl->GetFileName(book);
}

Flibrary::ILogicFactory::ExtractedBook BookExtractor::GetExtractedBook(const QString& bookId) const
{
	return m_impl->GetExtractedBook(bookId);
}
