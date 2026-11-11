#include "book.h"

#include <ranges>

#include "database/interface/IQuery.h"

#include "genre/Genre.h"
#include "util/StrUtil.h"

namespace {

constexpr auto booksQueryText = R"(
select b.BookID, f.FolderTitle, b.FileName || b.Ext, b.BookSize
    , a.LastName, a.FirstName, a.MiddleName
    , s.SeriesTitle, sl.SeqNumber
    , b.Title, gl.GenreCode, b.LibID, b.Lang
from Books b
join Folders f on f.FolderID = b.FolderID
join Author_List al on al.BookID = b.BookID
join Authors a on a.AuthorID = al.AuthorID
join Series_List sl on sl.BookID = b.BookID
join Series s on s.SeriesID = sl.SeriesID
join Genre_List gl on gl.BookID = b.BookID
)";

}

namespace HomeCompa::Test {

Util::ExtractedBooks GetExtractedBooks(DB::IDatabase& db)
{
	const auto genres       = Flibrary::Genre::Load(db);
	const auto genreParents = Flibrary::Genre::CreateGenreParentsMap(genres);

	const auto query = db.CreateQuery(booksQueryText);
	auto       books = *query | std::views::transform([&](const DB::IQuery& item) {
		Util::ExtractedBook         b {};
		Util::ExtractedBook::Author a {};
		QString                     genreCode {};
		std::tie(b.id, b.folder, b.file, b.size, a.lastName, a.firstName, a.middleName, b.series, b.seqNumber, b.title, genreCode, b.libId, b.lang) =
			item.Get<long long, QString, QString, size_t, QString, QString, QString, QString, int, QString, QString, long long, QString>();
		b.author = a.lastName;
		Util::AppendTitle(b.author, a.firstName);
		Util::AppendTitle(b.author, a.middleName);

		b.genre = Flibrary::Genre::Find(&genres, genreCode)->name;

		for (auto genre = b.genre; !genre.isEmpty(); genre = [&] {
				 const auto it = genreParents.find(genre);
				 return it != genreParents.end() ? it->second : QString {};
			 }())
			b.genreTree.push_front(genre);

		return b;
	}) | std::ranges::to<std::vector>();

	return books;
}

} // namespace HomeCompa::Test
