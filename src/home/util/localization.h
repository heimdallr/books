#pragma once

#include <qttranslation.h>
#include <QString>

#include "fnd/memory.h"

#include "export/Util.h"

class QTranslator;

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Loc::Ctx {

constexpr auto COMMON = "Common";
constexpr auto ERROR = "Error";
constexpr auto LANG = "Language";
constexpr auto BOOK = "Book";
constexpr auto LOGGING = "Logging";

}

namespace HomeCompa::Loc {

constexpr const char * LOCALES[]
{
	QT_TRANSLATE_NOOP("Language", "en"),
	QT_TRANSLATE_NOOP("Language", "ru"),
};

constexpr auto ERROR = QT_TRANSLATE_NOOP("Common", "Error!");
constexpr auto INFORMATION = QT_TRANSLATE_NOOP("Common", "Information");
constexpr auto QUESTION = QT_TRANSLATE_NOOP("Common", "Question");
constexpr auto WARNING = QT_TRANSLATE_NOOP("Common", "Warning!");

UTIL_EXPORT QString Tr(const char * context, const char * str);
UTIL_EXPORT QString GetLocale(const ISettings& settings);
UTIL_EXPORT std::vector<PropagateConstPtr<QTranslator>> LoadLocales(const ISettings & settings);

inline QString Error() { return Tr(Ctx::COMMON, ERROR); }
inline QString Information() { return Tr(Ctx::COMMON, INFORMATION); }
inline QString Question() { return Tr(Ctx::COMMON, QUESTION); }
inline QString Warning() { return Tr(Ctx::COMMON, WARNING); }

}

#define TR_DEF QString Tr(const char * str) {  return Loc::Tr(CONTEXT, str); }
