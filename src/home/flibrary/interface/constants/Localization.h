#pragma once

#include "util/localization.h"

namespace HomeCompa::Loc {

constexpr auto AUTHOR_NOT_SPECIFIED = QT_TRANSLATE_NOOP("Error", "Author not specified");
constexpr auto BOOKS_EXTRACT_ERROR = QT_TRANSLATE_NOOP("Error", "Retrieving books had errors");

#if(false)

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

#endif

constexpr auto NAVIGATION = "Navigation";
constexpr auto AUTHORS = QT_TRANSLATE_NOOP("Navigation", "Authors");
constexpr auto SERIES = QT_TRANSLATE_NOOP("Navigation", "Series");
constexpr auto GENRES = QT_TRANSLATE_NOOP("Navigation", "Genres");
constexpr auto KEYWORDS = QT_TRANSLATE_NOOP("Navigation", "Keywords");
constexpr auto ARCHIVES = QT_TRANSLATE_NOOP("Navigation", "Archives");
constexpr auto GROUPS = QT_TRANSLATE_NOOP("Navigation", "Groups");
constexpr auto SEARCH = QT_TRANSLATE_NOOP("Navigation", "Search");

}
