#include "BookView.h"

#include "database/interface/ICommand.h"
#include "database/interface/IDatabase.h"
#include "database/interface/ITransaction.h"

#include "interface/constants/SettingsConstant.h"
#include "interface/logic/IFilterProvider.h"

#include "util/ISettings.h"

namespace HomeCompa::Opds::BookView
{

namespace
{

constexpr auto DROP_BOOKS_VIEW   = "drop view if exists Books_View_Opds";
constexpr auto CREATE_BOOKS_VIEW = R"(
CREATE VIEW IF NOT EXISTS Books_View_Opds
(
	BookID,
	LibID,
	Title,
	SeriesID,
	SeqNumber,
	UpdateDate,
	LibRate,
	Lang,
	Year,
	FolderID,
	FileName,
	BookSize,
	UpdateID,
	IsDeleted,
	SearchTitle,
    BaseFileName,
    Ext
)
AS
{}
SELECT
		b.BookID,
		b.LibID,
		b.Title,
		b.SeriesID,
		b.SeqNumber,
		b.UpdateDate,
		nullif(coalesce(b.UserRate, b.LibRate), 0) as LibRate,
		b.Lang,
		b.Year,
		b.FolderID,
		b.FileName,
		b.BookSize,
		b.UpdateID,
		b.IsDeleted,
		b.SearchTitle,
		b.BaseFileName,
		b.Ext
	FROM Books_View b
	{}
	{})";

constexpr auto FILTERED_WITH = R"(with Filtered(BookID) as
(
	select b.BookID
	from Books b
	where not
	(      exists (select 42 from Languages m                  where m.LanguageCode = b.Lang                                and m.Flags & 2 != 0)
		or exists (select 42 from Authors   m join Author_List  l on l.AuthorID     = m.AuthorID  and l.BookID = b.BookID where m.Flags & 2 != 0)
		or exists (select 42 from Series    m join Series_List  l on l.SeriesID     = m.SeriesID  and l.BookID = b.BookID where m.Flags & 2 != 0)
		or exists (select 42 from Genres    m join Genre_List   l on l.GenreCode    = m.GenreCode and l.BookID = b.BookID where m.Flags & 2 != 0)
		or exists (select 42 from Keywords  m join Keyword_List l on l.KeywordID    = m.KeywordID and l.BookID = b.BookID where m.Flags & 2 != 0)
    )
))";

constexpr auto FILTERED_JOIN = "JOIN Filtered f on f.BookID = b.BookId";

std::string GetCreateBookViewCommandText(const ISettings& settings, const Flibrary::IFilterProvider& filterProvider) noexcept
{
	const auto hideRemoved   = !settings.Get(Flibrary::Constant::Settings::SHOW_REMOVED_BOOKS_KEY, false);
	const auto filterEnabled = filterProvider.IsFilterEnabled();

	std::string where;

	auto        append = [&](const std::string& what) {
        where += (where.empty() ? "" : " and ") + what;
	};

	if (hideRemoved)
		append("b.IsDeleted = 0");

	if (filterEnabled)
	{
		if (filterProvider.HideUnrated())
			append("b.LibRate is not null");

		if (filterProvider.IsMinimumRateEnabled())
			append(std::format("(b.LibRate >= {} or b.LibRate is null)", filterProvider.GetMinimumRate()));

		if (filterProvider.IsMaximumRateEnabled())
			append(std::format("(b.LibRate <= {} or b.LibRate is null)", filterProvider.GetMaximumRate()));
	}

	if (!where.empty())
		where = "WHERE " + where;

	return std::format(CREATE_BOOKS_VIEW, filterEnabled ? FILTERED_WITH : "", filterEnabled ? FILTERED_JOIN : "", where);
}

} // namespace

void Create(DB::IDatabase& db, const ISettings& settings, const Flibrary::IFilterProvider& filterProvider)
{
	const auto tr = db.CreateTransaction();
	tr->CreateCommand(DROP_BOOKS_VIEW)->Execute();
	tr->CreateCommand(GetCreateBookViewCommandText(settings, filterProvider))->Execute();
	tr->Commit();
}

} // namespace HomeCompa::Opds::BookView
