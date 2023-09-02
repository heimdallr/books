#pragma once

#include <qglobal.h>
#include <QCoreApplication>

#include "flint_export.h"

namespace HomeCompa::Flibrary::Loc::Ctx {
constexpr auto COMMON = "Common";
constexpr auto ERROR = "Error";
constexpr auto LANG = "Language";
constexpr auto BOOK = "Book";
constexpr auto LOGGING = "Logging";
}

namespace HomeCompa::Flibrary::Loc {

constexpr auto ERROR = QT_TRANSLATE_NOOP("Common", "Error!");
constexpr auto INFORMATION = QT_TRANSLATE_NOOP("Common", "Information");
constexpr auto QUESTION = QT_TRANSLATE_NOOP("Common", "Question");
constexpr auto WARNING = QT_TRANSLATE_NOOP("Common", "Warning!");

constexpr auto AUTHOR_NOT_SPECIFIED = QT_TRANSLATE_NOOP("Error", "Author not specified");

constexpr const char * LOCALES[]
{
	QT_TRANSLATE_NOOP("Language", "en"),
	QT_TRANSLATE_NOOP("Language", "ru"),
};

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
QT_TRANSLATE_NOOP("Book", "UpdateDate")
QT_TRANSLATE_NOOP("Book", "Lang")

#endif

FLINT_EXPORT QString Tr(const char * context, const char * str);

inline QString Error() { return Tr(Ctx::COMMON, ERROR); }
inline QString Information() { return Tr(Ctx::COMMON, INFORMATION); }
inline QString Question() { return Tr(Ctx::COMMON, QUESTION); }
inline QString Warning() { return Tr(Ctx::COMMON, WARNING); }

}

#define TR_DEF QString Tr(const char * str) {  return Loc::Tr(CONTEXT, str); }
