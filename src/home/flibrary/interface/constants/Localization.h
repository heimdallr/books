#pragma once

#include "util/localization.h"

namespace HomeCompa::Loc
{

constexpr auto AUTHOR_NOT_SPECIFIED = QT_TRANSLATE_NOOP("Error", "Author not specified");
constexpr auto BOOKS_EXTRACT_ERROR = QT_TRANSLATE_NOOP("Error", "Retrieving books had errors");

#if (false)

QT_TRANSLATE_NOOP("Book", "Author")
QT_TRANSLATE_NOOP("Book", "Title")
QT_TRANSLATE_NOOP("Book", "Series")
QT_TRANSLATE_NOOP("Book", "SeqNumber")
QT_TRANSLATE_NOOP("Book", "Size")
QT_TRANSLATE_NOOP("Book", "Genre")
QT_TRANSLATE_NOOP("Book", "Folder")
QT_TRANSLATE_NOOP("Book", "FileName")
QT_TRANSLATE_NOOP("Book", "LibRate")
QT_TRANSLATE_NOOP("Book", "UserRate")
QT_TRANSLATE_NOOP("Book", "UpdateDate")
QT_TRANSLATE_NOOP("Book", "Lang")
QT_TRANSLATE_NOOP("Book", "Year")

#endif

constexpr auto READER = QT_TRANSLATE_NOOP("Book", "Reader");
constexpr auto DATE_TIME = QT_TRANSLATE_NOOP("Book", "Date, Time");
constexpr auto COMMENT = QT_TRANSLATE_NOOP("Book", "Comment");

constexpr auto NAVIGATION = "Navigation";
constexpr auto Authors = QT_TRANSLATE_NOOP("Navigation", "Authors");
constexpr auto Series = QT_TRANSLATE_NOOP("Navigation", "Series");
constexpr auto Genres = QT_TRANSLATE_NOOP("Navigation", "Genres");
constexpr auto Keywords = QT_TRANSLATE_NOOP("Navigation", "Keywords");
constexpr auto Updates = QT_TRANSLATE_NOOP("Navigation", "Updates");
constexpr auto Archives = QT_TRANSLATE_NOOP("Navigation", "Archives");
constexpr auto Languages = QT_TRANSLATE_NOOP("Navigation", "Languages");
constexpr auto Groups = QT_TRANSLATE_NOOP("Navigation", "Groups");
constexpr auto Search = QT_TRANSLATE_NOOP("Navigation", "Search");
constexpr auto Reviews = QT_TRANSLATE_NOOP("Navigation", "Reviews");
constexpr auto AllBooks = QT_TRANSLATE_NOOP("Navigation", "AllBooks");

constexpr auto Books = QT_TRANSLATE_NOOP("Navigation", "Books");
constexpr auto AllOptions = QT_TRANSLATE_NOOP("Navigation", "All options");

constexpr auto AUTHORS = QT_TRANSLATE_NOOP("Annotation", "Authors:");
constexpr auto SERIES = QT_TRANSLATE_NOOP("Annotation", "Series:");
constexpr auto GENRES = QT_TRANSLATE_NOOP("Annotation", "Genres:");
constexpr auto ARCHIVE = QT_TRANSLATE_NOOP("Annotation", "Archives:");
constexpr auto GROUPS = QT_TRANSLATE_NOOP("Annotation", "Groups:");
constexpr auto KEYWORDS = QT_TRANSLATE_NOOP("Annotation", "Keywords:");
constexpr auto UPDATES = QT_TRANSLATE_NOOP("Annotation", "Updates:");
constexpr auto LANGUAGE = QT_TRANSLATE_NOOP("Annotation", "Languages:");
constexpr auto RATE = QT_TRANSLATE_NOOP("Annotation", "Rate:");
constexpr auto USER_RATE = QT_TRANSLATE_NOOP("Annotation", "My rate:");

constexpr auto EXPORT = "Export";
constexpr auto SELECT_INPX_FILE = QT_TRANSLATE_NOOP("Export", "Save index file");
constexpr auto SELECT_INPX_FILE_FILTER = QT_TRANSLATE_NOOP("Export", "Index files (*.inpx);;All files (*.*)");

} // namespace HomeCompa::Loc
