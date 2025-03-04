#pragma once

#include <qttranslation.h>

#include <QString>

#include "fnd/memory.h"

#include "export/Util.h"

class QTranslator;

namespace HomeCompa
{
class ISettings;
}

namespace HomeCompa::Loc::Ctx
{

constexpr auto COMMON = "Common";
constexpr auto ERROR = "Error";
constexpr auto LANG = "Language";
constexpr auto BOOK = "Book";
constexpr auto LOGGING = "Logging";

}

namespace HomeCompa::Loc
{

constexpr const char* LOCALES[] {
	QT_TRANSLATE_NOOP("Language", "en"),
	QT_TRANSLATE_NOOP("Language", "ru"),
	QT_TRANSLATE_NOOP("Language", "uk"),
};

constexpr auto ERROR = QT_TRANSLATE_NOOP("Common", "Error!");
constexpr auto INFORMATION = QT_TRANSLATE_NOOP("Common", "Information");
constexpr auto QUESTION = QT_TRANSLATE_NOOP("Common", "Question");
constexpr auto WARNING = QT_TRANSLATE_NOOP("Common", "Warning!");
constexpr auto CONFIRM_RESTART = QT_TRANSLATE_NOOP("Common", "You must restart the application to apply the changes.\nRestart now?");

UTIL_EXPORT QString Tr(const char* context, const char* str);
UTIL_EXPORT std::vector<const char*> GetLocales();
UTIL_EXPORT QString GetLocale(const ISettings& settings);
UTIL_EXPORT std::vector<PropagateConstPtr<QTranslator>> LoadLocales(const ISettings& settings);
UTIL_EXPORT std::vector<PropagateConstPtr<QTranslator>> LoadLocales(const QString& locale);

inline QString Error()
{
	return Tr(Ctx::COMMON, ERROR);
}

inline QString Information()
{
	return Tr(Ctx::COMMON, INFORMATION);
}

inline QString Question()
{
	return Tr(Ctx::COMMON, QUESTION);
}

inline QString Warning()
{
	return Tr(Ctx::COMMON, WARNING);
}

} // namespace HomeCompa::Loc

namespace HomeCompa
{

constexpr auto LANGUAGES_CONTEXT = "Language";
constexpr std::pair<const char*, const char*> LANGUAGES[] {
	{ "am", QT_TRANSLATE_NOOP("Language", "Amharic") },
	{ "ar", QT_TRANSLATE_NOOP("Language", "Arabic") },
	{ "az", QT_TRANSLATE_NOOP("Language", "Azerbaijani") },
	{ "ba", QT_TRANSLATE_NOOP("Language", "Bashkir") },
	{ "be", QT_TRANSLATE_NOOP("Language", "Belarusian") },
	{ "bg", QT_TRANSLATE_NOOP("Language", "Bulgarian") },
	{ "bn", QT_TRANSLATE_NOOP("Language", "Bengali") },
	{ "ca", QT_TRANSLATE_NOOP("Language", "Catalan") },
	{ "cs", QT_TRANSLATE_NOOP("Language", "Czech") },
	{ "cu", QT_TRANSLATE_NOOP("Language", "Church Slavonic") },
	{ "cv", QT_TRANSLATE_NOOP("Language", "Chuvash") },
	{ "da", QT_TRANSLATE_NOOP("Language", "Danish") },
	{ "de", QT_TRANSLATE_NOOP("Language", "German") },
	{ "el", QT_TRANSLATE_NOOP("Language", "Greek") },
	{ "en", QT_TRANSLATE_NOOP("Language", "English") },
	{ "eo", QT_TRANSLATE_NOOP("Language", "Esperanto") },
	{ "es", QT_TRANSLATE_NOOP("Language", "Spanish") },
	{ "et", QT_TRANSLATE_NOOP("Language", "Estonian") },
	{ "fi", QT_TRANSLATE_NOOP("Language", "Finnish") },
	{ "fr", QT_TRANSLATE_NOOP("Language", "French") },
	{ "ga", QT_TRANSLATE_NOOP("Language", "Irish") },
	{ "gu", QT_TRANSLATE_NOOP("Language", "Gujarati") },
	{ "he", QT_TRANSLATE_NOOP("Language", "Hebrew") },
	{ "hi", QT_TRANSLATE_NOOP("Language", "Hindi") },
	{ "hr", QT_TRANSLATE_NOOP("Language", "Croatian") },
	{ "hu", QT_TRANSLATE_NOOP("Language", "Hungarian") },
	{ "hy", QT_TRANSLATE_NOOP("Language", "Armenian") },
	{ "id", QT_TRANSLATE_NOOP("Language", "Indonesian") },
	{ "ie", QT_TRANSLATE_NOOP("Language", "Lombard") },
	{ "is", QT_TRANSLATE_NOOP("Language", "Icelandic") },
	{ "it", QT_TRANSLATE_NOOP("Language", "Italian") },
	{ "ja", QT_TRANSLATE_NOOP("Language", "Japan") },
	{ "ka", QT_TRANSLATE_NOOP("Language", "Chechen") },
	{ "kk", QT_TRANSLATE_NOOP("Language", "Kazakh") },
	{ "kn", QT_TRANSLATE_NOOP("Language", "Kannada") },
	{ "ko", QT_TRANSLATE_NOOP("Language", "Korean") },
	{ "ks", QT_TRANSLATE_NOOP("Language", "Kashmiri") },
	{ "ku", QT_TRANSLATE_NOOP("Language", "Kurdish") },
	{ "ky", QT_TRANSLATE_NOOP("Language", "Kyrgyz") },
	{ "la", QT_TRANSLATE_NOOP("Language", "Latin") },
	{ "lt", QT_TRANSLATE_NOOP("Language", "Lithuanian") },
	{ "lv", QT_TRANSLATE_NOOP("Language", "Latvian") },
	{ "mk", QT_TRANSLATE_NOOP("Language", "Macedonian") },
	{ "ml", QT_TRANSLATE_NOOP("Language", "Malayalam") },
	{ "mr", QT_TRANSLATE_NOOP("Language", "Marathi") },
	{ "my", QT_TRANSLATE_NOOP("Language", "Burmese") },
	{ "ne", QT_TRANSLATE_NOOP("Language", "Neo") },
	{ "nl", QT_TRANSLATE_NOOP("Language", "Dutch") },
	{ "no", QT_TRANSLATE_NOOP("Language", "Norwegian") },
	{ "pl", QT_TRANSLATE_NOOP("Language", "Polish") },
	{ "ps", QT_TRANSLATE_NOOP("Language", "Pashto") },
	{ "pt", QT_TRANSLATE_NOOP("Language", "Portuguese") },
	{ "ro", QT_TRANSLATE_NOOP("Language", "Romanian") },
	{ "ru", QT_TRANSLATE_NOOP("Language", "Russian") },
	{ "sah", QT_TRANSLATE_NOOP("Language", "Yakut") },
	{ "sd", QT_TRANSLATE_NOOP("Language", "Sindhi") },
	{ "sk", QT_TRANSLATE_NOOP("Language", "Slovak") },
	{ "sq", "sq" },
	{ "sr", QT_TRANSLATE_NOOP("Language", "Serbian") },
	{ "sv", QT_TRANSLATE_NOOP("Language", "Swedish") },
	{ "ta", QT_TRANSLATE_NOOP("Language", "Tamil") },
	{ "te", QT_TRANSLATE_NOOP("Language", "Telugu") },
	{ "tg", QT_TRANSLATE_NOOP("Language", "Tajik") },
	{ "tr", QT_TRANSLATE_NOOP("Language", "Turkish") },
	{ "tt", QT_TRANSLATE_NOOP("Language", "Tatar") },
	{ "ug", QT_TRANSLATE_NOOP("Language", "Uigur") },
	{ "uk", QT_TRANSLATE_NOOP("Language", "Ukrainian") },
	{ "un", QT_TRANSLATE_NOOP("Language", "Undefined") },
	{ "ur", QT_TRANSLATE_NOOP("Language", "Urdu") },
	{ "uz", QT_TRANSLATE_NOOP("Language", "Uzbek") },
	{ "vi", QT_TRANSLATE_NOOP("Language", "Vietnamese") },
	{ "xx", "xx" },
	{ "zh", QT_TRANSLATE_NOOP("Language", "Chinese (Simplified)") },
};

}

#define TR_DEF                        \
	QString Tr(const char* str)       \
	{                                 \
		return Loc::Tr(CONTEXT, str); \
	}
