#pragma once

#include <QHash>
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

constexpr auto COMMON    = "Common";
constexpr auto ERROR_CTX = "Error";
constexpr auto LANG      = "Language";
constexpr auto BOOK      = "Book";
constexpr auto LOGGING   = "Logging";

}

namespace HomeCompa::Loc
{

constexpr auto ERROR_TEXT      = QT_TRANSLATE_NOOP("Common", "Error!");
constexpr auto INFORMATION     = QT_TRANSLATE_NOOP("Common", "Information");
constexpr auto QUESTION        = QT_TRANSLATE_NOOP("Common", "Question");
constexpr auto WARNING         = QT_TRANSLATE_NOOP("Common", "Warning!");
constexpr auto CONFIRM_RESTART = QT_TRANSLATE_NOOP("Common", "You must restart the application to apply the changes.\nRestart now?");
constexpr auto ANONYMOUS       = QT_TRANSLATE_NOOP("Common", "Anonymous");

UTIL_EXPORT QString Tr(const char* context, const char* str);
UTIL_EXPORT std::vector<const char*> GetLocales();
UTIL_EXPORT QString                  GetLocale(const ISettings& settings);
UTIL_EXPORT std::vector<PropagateConstPtr<QTranslator>> LoadLocales(const ISettings& settings);
UTIL_EXPORT std::vector<PropagateConstPtr<QTranslator>> LoadLocales(const QString& locale);

inline QString Error()
{
	return Tr(Ctx::COMMON, ERROR_TEXT);
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

constexpr auto MONTHS_CONTEXT = "Month";
#if 0
QT_TRANSLATE_NOOP("Month", "1")
QT_TRANSLATE_NOOP("Month", "2")
QT_TRANSLATE_NOOP("Month", "3")
QT_TRANSLATE_NOOP("Month", "4")
QT_TRANSLATE_NOOP("Month", "5")
QT_TRANSLATE_NOOP("Month", "6")
QT_TRANSLATE_NOOP("Month", "7")
QT_TRANSLATE_NOOP("Month", "8")
QT_TRANSLATE_NOOP("Month", "9")
QT_TRANSLATE_NOOP("Month", "10")
QT_TRANSLATE_NOOP("Month", "11")
QT_TRANSLATE_NOOP("Month", "12")
QT_TRANSLATE_NOOP("Month", "2147483647")
#endif

constexpr auto LANGUAGES_CONTEXT = "Language";
constexpr auto UNDEFINED_KEY     = "un";
constexpr auto UNDEFINED         = QT_TRANSLATE_NOOP("Language", "[Undetermined]");

struct Language
{
	const char* key { nullptr };
	const char* title { nullptr };
	int         priority { std::numeric_limits<int>::max() };
};

constexpr Language LANGUAGES[] {
	{ UNDEFINED_KEY, UNDEFINED, 10000 },
	{ "aa", QT_TRANSLATE_NOOP("Language", "Afar") },
	{ "ab", QT_TRANSLATE_NOOP("Language", "Abkhazian") },
	{ "ad", QT_TRANSLATE_NOOP("Language", "Adygei") },
	{ "ae", QT_TRANSLATE_NOOP("Language", "Avestan") },
	{ "af", QT_TRANSLATE_NOOP("Language", "Afrikaans") },
	{ "ak", QT_TRANSLATE_NOOP("Language", "Akan") },
	{ "am", QT_TRANSLATE_NOOP("Language", "Amharic") },
	{ "an", QT_TRANSLATE_NOOP("Language", "Aragonese") },
	{ "ar", QT_TRANSLATE_NOOP("Language", "Arabic") },
	{ "as", QT_TRANSLATE_NOOP("Language", "Assamese") },
	{ "av", QT_TRANSLATE_NOOP("Language", "Avaric") },
	{ "ay", QT_TRANSLATE_NOOP("Language", "Aymara") },
	{ "az", QT_TRANSLATE_NOOP("Language", "Azerbaijani") },
	{ "ba", QT_TRANSLATE_NOOP("Language", "Bashkir") },
	{ "be", QT_TRANSLATE_NOOP("Language", "Belarusian"), 1000 },
	{ "bg", QT_TRANSLATE_NOOP("Language", "Bulgarian") },
	{ "bi", QT_TRANSLATE_NOOP("Language", "Bislama") },
	{ "bm", QT_TRANSLATE_NOOP("Language", "Bambara") },
	{ "bn", QT_TRANSLATE_NOOP("Language", "Bengali") },
	{ "bo", QT_TRANSLATE_NOOP("Language", "Tibetan") },
	{ "br", QT_TRANSLATE_NOOP("Language", "Breton") },
	{ "bs", QT_TRANSLATE_NOOP("Language", "Bosnian") },
	{ "ca", QT_TRANSLATE_NOOP("Language", "Catalan") },
	{ "ce", QT_TRANSLATE_NOOP("Language", "Chechen") },
	{ "ch", QT_TRANSLATE_NOOP("Language", "Chamorro") },
	{ "co", QT_TRANSLATE_NOOP("Language", "Corsican") },
	{ "cr", QT_TRANSLATE_NOOP("Language", "Cree") },
	{ "cs", QT_TRANSLATE_NOOP("Language", "Czech") },
	{ "cu", QT_TRANSLATE_NOOP("Language", "Church Slavic") },
	{ "cv", QT_TRANSLATE_NOOP("Language", "Chuvash") },
	{ "cy", QT_TRANSLATE_NOOP("Language", "Welsh") },
	{ "da", QT_TRANSLATE_NOOP("Language", "Danish") },
	{ "de", QT_TRANSLATE_NOOP("Language", "German"), 1000 },
	{ "dv", QT_TRANSLATE_NOOP("Language", "Divehi") },
	{ "dz", QT_TRANSLATE_NOOP("Language", "Dzongkha") },
	{ "ee", QT_TRANSLATE_NOOP("Language", "Ewe") },
	{ "el", QT_TRANSLATE_NOOP("Language", "Greek") },
	{ "en", QT_TRANSLATE_NOOP("Language", "English"), 100 },
	{ "eo", QT_TRANSLATE_NOOP("Language", "Esperanto") },
	{ "es", QT_TRANSLATE_NOOP("Language", "Spanish"), 1000 },
	{ "et", QT_TRANSLATE_NOOP("Language", "Estonian") },
	{ "eu", QT_TRANSLATE_NOOP("Language", "Basque") },
	{ "fa", QT_TRANSLATE_NOOP("Language", "Persian") },
	{ "ff", QT_TRANSLATE_NOOP("Language", "Fulah") },
	{ "fi", QT_TRANSLATE_NOOP("Language", "Finnish") },
	{ "fj", QT_TRANSLATE_NOOP("Language", "Fijian") },
	{ "fo", QT_TRANSLATE_NOOP("Language", "Faroese") },
	{ "fr", QT_TRANSLATE_NOOP("Language", "French"), 1000 },
	{ "fy", QT_TRANSLATE_NOOP("Language", "Western Frisian") },
	{ "ga", QT_TRANSLATE_NOOP("Language", "Irish") },
	{ "gd", QT_TRANSLATE_NOOP("Language", "Gaelic") },
	{ "gl", QT_TRANSLATE_NOOP("Language", "Galician") },
	{ "gn", QT_TRANSLATE_NOOP("Language", "Guarani") },
	{ "gu", QT_TRANSLATE_NOOP("Language", "Gujarati") },
	{ "gv", QT_TRANSLATE_NOOP("Language", "Manx") },
	{ "ha", QT_TRANSLATE_NOOP("Language", "Hausa") },
	{ "he", QT_TRANSLATE_NOOP("Language", "Hebrew") },
	{ "hi", QT_TRANSLATE_NOOP("Language", "Hindi") },
	{ "ho", QT_TRANSLATE_NOOP("Language", "Hiri Motu") },
	{ "hr", QT_TRANSLATE_NOOP("Language", "Croatian") },
	{ "ht", QT_TRANSLATE_NOOP("Language", "Haitian") },
	{ "hu", QT_TRANSLATE_NOOP("Language", "Hungarian") },
	{ "hy", QT_TRANSLATE_NOOP("Language", "Armenian") },
	{ "hz", QT_TRANSLATE_NOOP("Language", "Herero") },
	{ "ia", QT_TRANSLATE_NOOP("Language", "Interlingua") },
	{ "id", QT_TRANSLATE_NOOP("Language", "Indonesian") },
	{ "ie", QT_TRANSLATE_NOOP("Language", "Interlingue") },
	{ "ig", QT_TRANSLATE_NOOP("Language", "Igbo") },
	{ "ii", QT_TRANSLATE_NOOP("Language", "Sichuan Yi") },
	{ "ik", QT_TRANSLATE_NOOP("Language", "Inupiaq") },
	{ "io", QT_TRANSLATE_NOOP("Language", "Ido") },
	{ "is", QT_TRANSLATE_NOOP("Language", "Icelandic") },
	{ "it", QT_TRANSLATE_NOOP("Language", "Italian") },
	{ "iu", QT_TRANSLATE_NOOP("Language", "Inuktitut") },
	{ "ja", QT_TRANSLATE_NOOP("Language", "Japanese") },
	{ "jv", QT_TRANSLATE_NOOP("Language", "Javanese") },
	{ "ka", QT_TRANSLATE_NOOP("Language", "Georgian") },
	{ "kg", QT_TRANSLATE_NOOP("Language", "Kongo") },
	{ "ki", QT_TRANSLATE_NOOP("Language", "Kikuyu") },
	{ "kj", QT_TRANSLATE_NOOP("Language", "Kuanyama") },
	{ "kk", QT_TRANSLATE_NOOP("Language", "Kazakh") },
	{ "kl", QT_TRANSLATE_NOOP("Language", "Greenlandic") },
	{ "km", QT_TRANSLATE_NOOP("Language", "Central Khmer") },
	{ "kn", QT_TRANSLATE_NOOP("Language", "Kannada") },
	{ "ko", QT_TRANSLATE_NOOP("Language", "Korean") },
	{ "kr", QT_TRANSLATE_NOOP("Language", "Kanuri") },
	{ "ks", QT_TRANSLATE_NOOP("Language", "Kashmiri") },
	{ "ku", QT_TRANSLATE_NOOP("Language", "Kurdish") },
	{ "kv", QT_TRANSLATE_NOOP("Language", "Komi") },
	{ "kw", QT_TRANSLATE_NOOP("Language", "Cornish") },
	{ "ky", QT_TRANSLATE_NOOP("Language", "Kyrgyz") },
	{ "la", QT_TRANSLATE_NOOP("Language", "Latin") },
	{ "lb", QT_TRANSLATE_NOOP("Language", "Luxembourgish") },
	{ "lg", QT_TRANSLATE_NOOP("Language", "Ganda") },
	{ "li", QT_TRANSLATE_NOOP("Language", "Limburgish") },
	{ "ln", QT_TRANSLATE_NOOP("Language", "Lingala") },
	{ "lo", QT_TRANSLATE_NOOP("Language", "Lao") },
	{ "lt", QT_TRANSLATE_NOOP("Language", "Lithuanian") },
	{ "lu", QT_TRANSLATE_NOOP("Language", "Luba-Katanga") },
	{ "lv", QT_TRANSLATE_NOOP("Language", "Latvian") },
	{ "mg", QT_TRANSLATE_NOOP("Language", "Malagasy") },
	{ "mh", QT_TRANSLATE_NOOP("Language", "Marshallese") },
	{ "mi", QT_TRANSLATE_NOOP("Language", "Maori") },
	{ "mk", QT_TRANSLATE_NOOP("Language", "Macedonian") },
	{ "ml", QT_TRANSLATE_NOOP("Language", "Malayalam") },
	{ "mn", QT_TRANSLATE_NOOP("Language", "Mongolian") },
	{ "mr", QT_TRANSLATE_NOOP("Language", "Marathi") },
	{ "ms", QT_TRANSLATE_NOOP("Language", "Malay") },
	{ "mt", QT_TRANSLATE_NOOP("Language", "Maltese") },
	{ "my", QT_TRANSLATE_NOOP("Language", "Burmese") },
	{ "na", QT_TRANSLATE_NOOP("Language", "Nauru") },
	{ "nb", QT_TRANSLATE_NOOP("Language", "Norwegian Bokmål") },
	{ "nd", QT_TRANSLATE_NOOP("Language", "North Ndebele") },
	{ "ne", QT_TRANSLATE_NOOP("Language", "Nepali") },
	{ "ng", QT_TRANSLATE_NOOP("Language", "Ndonga") },
	{ "nl", QT_TRANSLATE_NOOP("Language", "Dutch"), 1000 },
	{ "nn", QT_TRANSLATE_NOOP("Language", "Norwegian Nynorsk") },
	{ "no", QT_TRANSLATE_NOOP("Language", "Norwegian") },
	{ "nr", QT_TRANSLATE_NOOP("Language", "South Ndebele") },
	{ "nv", QT_TRANSLATE_NOOP("Language", "Navajo") },
	{ "ny", QT_TRANSLATE_NOOP("Language", "Nyanja") },
	{ "oc", QT_TRANSLATE_NOOP("Language", "Occitan") },
	{ "oj", QT_TRANSLATE_NOOP("Language", "Ojibwa") },
	{ "om", QT_TRANSLATE_NOOP("Language", "Oromo") },
	{ "or", QT_TRANSLATE_NOOP("Language", "Oriya") },
	{ "os", QT_TRANSLATE_NOOP("Language", "Ossetian") },
	{ "pa", QT_TRANSLATE_NOOP("Language", "Panjabi") },
	{ "pi", QT_TRANSLATE_NOOP("Language", "Pali") },
	{ "pl", QT_TRANSLATE_NOOP("Language", "Polish"), 1000 },
	{ "ps", QT_TRANSLATE_NOOP("Language", "Pushto") },
	{ "pt", QT_TRANSLATE_NOOP("Language", "Portuguese"), 1000 },
	{ "qu", QT_TRANSLATE_NOOP("Language", "Quechua") },
	{ "rm", QT_TRANSLATE_NOOP("Language", "Romansh") },
	{ "rn", QT_TRANSLATE_NOOP("Language", "Rundi") },
	{ "ro", QT_TRANSLATE_NOOP("Language", "Romanian, Moldavian") },
	{ "ru", QT_TRANSLATE_NOOP("Language", "Russian"), 100 },
	{ "rw", QT_TRANSLATE_NOOP("Language", "Kinyarwanda") },
	{ "sa", QT_TRANSLATE_NOOP("Language", "Sanskrit") },
	{ "sc", QT_TRANSLATE_NOOP("Language", "Sardinian") },
	{ "sd", QT_TRANSLATE_NOOP("Language", "Sindhi") },
	{ "se", QT_TRANSLATE_NOOP("Language", "Northern Sami") },
	{ "sg", QT_TRANSLATE_NOOP("Language", "Sango") },
	{ "si", QT_TRANSLATE_NOOP("Language", "Sinhalese") },
	{ "sk", QT_TRANSLATE_NOOP("Language", "Slovak") },
	{ "sl", QT_TRANSLATE_NOOP("Language", "Slovenian") },
	{ "sm", QT_TRANSLATE_NOOP("Language", "Samoan") },
	{ "sn", QT_TRANSLATE_NOOP("Language", "Shona") },
	{ "so", QT_TRANSLATE_NOOP("Language", "Somali") },
	{ "sq", QT_TRANSLATE_NOOP("Language", "Albanian") },
	{ "sr", QT_TRANSLATE_NOOP("Language", "Serbian") },
	{ "ss", QT_TRANSLATE_NOOP("Language", "Swati") },
	{ "st", QT_TRANSLATE_NOOP("Language", "Southern Sotho") },
	{ "su", QT_TRANSLATE_NOOP("Language", "Sundanese") },
	{ "sv", QT_TRANSLATE_NOOP("Language", "Swedish") },
	{ "sw", QT_TRANSLATE_NOOP("Language", "Swahili") },
	{ "ta", QT_TRANSLATE_NOOP("Language", "Tamil") },
	{ "te", QT_TRANSLATE_NOOP("Language", "Telugu") },
	{ "tg", QT_TRANSLATE_NOOP("Language", "Tajik") },
	{ "th", QT_TRANSLATE_NOOP("Language", "Thai") },
	{ "ti", QT_TRANSLATE_NOOP("Language", "Tigrinya") },
	{ "tk", QT_TRANSLATE_NOOP("Language", "Turkmen") },
	{ "tl", QT_TRANSLATE_NOOP("Language", "Tagalog") },
	{ "tn", QT_TRANSLATE_NOOP("Language", "Tswana") },
	{ "to", QT_TRANSLATE_NOOP("Language", "Tonga Islands") },
	{ "tr", QT_TRANSLATE_NOOP("Language", "Turkish") },
	{ "ts", QT_TRANSLATE_NOOP("Language", "Tsonga") },
	{ "tt", QT_TRANSLATE_NOOP("Language", "Tatar") },
	{ "tw", QT_TRANSLATE_NOOP("Language", "Twi") },
	{ "ty", QT_TRANSLATE_NOOP("Language", "Tahitian") },
	{ "ug", QT_TRANSLATE_NOOP("Language", "Uighur") },
	{ "uk", QT_TRANSLATE_NOOP("Language", "Ukrainian"), 100 },
	{ "ur", QT_TRANSLATE_NOOP("Language", "Urdu") },
	{ "uz", QT_TRANSLATE_NOOP("Language", "Uzbek") },
	{ "ve", QT_TRANSLATE_NOOP("Language", "Venda") },
	{ "vi", QT_TRANSLATE_NOOP("Language", "Vietnamese") },
	{ "vo", QT_TRANSLATE_NOOP("Language", "Volapuk") },
	{ "wa", QT_TRANSLATE_NOOP("Language", "Walloon") },
	{ "wo", QT_TRANSLATE_NOOP("Language", "Wolof") },
	{ "xh", QT_TRANSLATE_NOOP("Language", "Xhosa") },
	{ "yi", QT_TRANSLATE_NOOP("Language", "Yiddish") },
	{ "yo", QT_TRANSLATE_NOOP("Language", "Yoruba") },
	{ "za", QT_TRANSLATE_NOOP("Language", "Zhuang") },
	{ "zh", QT_TRANSLATE_NOOP("Language", "Chinese") },
	{ "zu", QT_TRANSLATE_NOOP("Language", "Zulu") },
	{ "ace", QT_TRANSLATE_NOOP("Language", "Achinese") },
	{ "ach", QT_TRANSLATE_NOOP("Language", "Acoli") },
	{ "ada", QT_TRANSLATE_NOOP("Language", "Adangme") },
	{ "afh", QT_TRANSLATE_NOOP("Language", "Afrihili") },
	{ "ain", QT_TRANSLATE_NOOP("Language", "Ainu") },
	{ "akk", QT_TRANSLATE_NOOP("Language", "Akkadian") },
	{ "ale", QT_TRANSLATE_NOOP("Language", "Aleut") },
	{ "egy", QT_TRANSLATE_NOOP("Language", "Ancient Egyptian") },
	{ "grc", QT_TRANSLATE_NOOP("Language", "Ancient Greek") },
	{ "anp", QT_TRANSLATE_NOOP("Language", "Angika") },
	{ "arp", QT_TRANSLATE_NOOP("Language", "Arapaho") },
	{ "arw", QT_TRANSLATE_NOOP("Language", "Arawak") },
	{ "rup", QT_TRANSLATE_NOOP("Language", "Aromanian") },
	{ "ast", QT_TRANSLATE_NOOP("Language", "Asturleonese") },
	{ "den", QT_TRANSLATE_NOOP("Language", "Athapascan") },
	{ "awa", QT_TRANSLATE_NOOP("Language", "Awadhi") },
	{ "ban", QT_TRANSLATE_NOOP("Language", "Balinese") },
	{ "bal", QT_TRANSLATE_NOOP("Language", "Baluchi") },
	{ "bas", QT_TRANSLATE_NOOP("Language", "Basa") },
	{ "bej", QT_TRANSLATE_NOOP("Language", "Beja") },
	{ "bem", QT_TRANSLATE_NOOP("Language", "Bemba") },
	{ "bho", QT_TRANSLATE_NOOP("Language", "Bhojpuri") },
	{ "bik", QT_TRANSLATE_NOOP("Language", "Bikol") },
	{ "bin", QT_TRANSLATE_NOOP("Language", "Bini") },
	{ "byn", QT_TRANSLATE_NOOP("Language", "Bilen") },
	{ "zbl", QT_TRANSLATE_NOOP("Language", "Blissymbolics") },
	{ "bra", QT_TRANSLATE_NOOP("Language", "Braj") },
	{ "bug", QT_TRANSLATE_NOOP("Language", "Buginese") },
	{ "bua", QT_TRANSLATE_NOOP("Language", "Buriat") },
	{ "cad", QT_TRANSLATE_NOOP("Language", "Caddo") },
	{ "ceb", QT_TRANSLATE_NOOP("Language", "Cebuano") },
	{ "chg", QT_TRANSLATE_NOOP("Language", "Chagatai") },
	{ "chr", QT_TRANSLATE_NOOP("Language", "Cherokee") },
	{ "chy", QT_TRANSLATE_NOOP("Language", "Cheyenne") },
	{ "chb", QT_TRANSLATE_NOOP("Language", "Chibcha") },
	{ "chn", QT_TRANSLATE_NOOP("Language", "Chinook") },
	{ "chp", QT_TRANSLATE_NOOP("Language", "Chipewyan") },
	{ "cho", QT_TRANSLATE_NOOP("Language", "Choctaw") },
	{ "chk", QT_TRANSLATE_NOOP("Language", "Chuukese") },
	{ "syc", QT_TRANSLATE_NOOP("Language", "Classical Syriac") },
	{ "cop", QT_TRANSLATE_NOOP("Language", "Coptic") },
	{ "mus", QT_TRANSLATE_NOOP("Language", "Creek") },
	{ "crh", QT_TRANSLATE_NOOP("Language", "Crimean Tatar") },
	{ "dak", QT_TRANSLATE_NOOP("Language", "Dakota") },
	{ "dar", QT_TRANSLATE_NOOP("Language", "Dargwa") },
	{ "del", QT_TRANSLATE_NOOP("Language", "Delaware") },
	{ "din", QT_TRANSLATE_NOOP("Language", "Dinka") },
	{ "doi", QT_TRANSLATE_NOOP("Language", "Dogri") },
	{ "dgr", QT_TRANSLATE_NOOP("Language", "Dogrib") },
	{ "dua", QT_TRANSLATE_NOOP("Language", "Duala") },
	{ "dyu", QT_TRANSLATE_NOOP("Language", "Dyula") },
	{ "frs", QT_TRANSLATE_NOOP("Language", "Eastern Frisian") },
	{ "efi", QT_TRANSLATE_NOOP("Language", "Efik") },
	{ "eka", QT_TRANSLATE_NOOP("Language", "Ekajuk") },
	{ "elx", QT_TRANSLATE_NOOP("Language", "Elamite") },
	{ "myv", QT_TRANSLATE_NOOP("Language", "Erzya") },
	{ "ewo", QT_TRANSLATE_NOOP("Language", "Ewondo") },
	{ "fan", QT_TRANSLATE_NOOP("Language", "Fang") },
	{ "fat", QT_TRANSLATE_NOOP("Language", "Fanti") },
	{ "fil", QT_TRANSLATE_NOOP("Language", "Filipino") },
	{ "fon", QT_TRANSLATE_NOOP("Language", "Fon") },
	{ "fur", QT_TRANSLATE_NOOP("Language", "Friulian") },
	{ "gaa", QT_TRANSLATE_NOOP("Language", "Ga") },
	{ "car", QT_TRANSLATE_NOOP("Language", "Galibi Carib") },
	{ "gay", QT_TRANSLATE_NOOP("Language", "Gayo") },
	{ "gba", QT_TRANSLATE_NOOP("Language", "Gbaya") },
	{ "gez", QT_TRANSLATE_NOOP("Language", "Geez") },
	{ "gil", QT_TRANSLATE_NOOP("Language", "Gilbertese") },
	{ "gon", QT_TRANSLATE_NOOP("Language", "Gondi") },
	{ "gor", QT_TRANSLATE_NOOP("Language", "Gorontalo") },
	{ "got", QT_TRANSLATE_NOOP("Language", "Gothic") },
	{ "grb", QT_TRANSLATE_NOOP("Language", "Grebo") },
	{ "gwi", QT_TRANSLATE_NOOP("Language", "Gwich'in") },
	{ "hai", QT_TRANSLATE_NOOP("Language", "Haida") },
	{ "haw", QT_TRANSLATE_NOOP("Language", "Hawaiian") },
	{ "hil", QT_TRANSLATE_NOOP("Language", "Hiligaynon") },
	{ "hit", QT_TRANSLATE_NOOP("Language", "Hittite") },
	{ "hmn", QT_TRANSLATE_NOOP("Language", "Hmong") },
	{ "hup", QT_TRANSLATE_NOOP("Language", "Hupa") },
	{ "iba", QT_TRANSLATE_NOOP("Language", "Iban") },
	{ "ilo", QT_TRANSLATE_NOOP("Language", "Iloko") },
	{ "arc", QT_TRANSLATE_NOOP("Language", "Imperial Aramaic") },
	{ "smn", QT_TRANSLATE_NOOP("Language", "Inari Sami") },
	{ "inh", QT_TRANSLATE_NOOP("Language", "Ingush") },
	{ "jrb", QT_TRANSLATE_NOOP("Language", "Judeo-Arabic") },
	{ "jpr", QT_TRANSLATE_NOOP("Language", "Judeo-Persian") },
	{ "kbd", QT_TRANSLATE_NOOP("Language", "Kabardian") },
	{ "kab", QT_TRANSLATE_NOOP("Language", "Kabyle") },
	{ "kac", QT_TRANSLATE_NOOP("Language", "Kachin") },
	{ "xal", QT_TRANSLATE_NOOP("Language", "Kalmyk") },
	{ "kam", QT_TRANSLATE_NOOP("Language", "Kamba") },
	{ "kaa", QT_TRANSLATE_NOOP("Language", "Kara-Kalpak") },
	{ "krc", QT_TRANSLATE_NOOP("Language", "Karachay-Balkar") },
	{ "krl", QT_TRANSLATE_NOOP("Language", "Karelian") },
	{ "csb", QT_TRANSLATE_NOOP("Language", "Kashubian") },
	{ "kaw", QT_TRANSLATE_NOOP("Language", "Kawi") },
	{ "kha", QT_TRANSLATE_NOOP("Language", "Khasi") },
	{ "kho", QT_TRANSLATE_NOOP("Language", "Khotanese") },
	{ "kmb", QT_TRANSLATE_NOOP("Language", "Kimbundu") },
	{ "tlh", QT_TRANSLATE_NOOP("Language", "Klingon") },
	{ "kok", QT_TRANSLATE_NOOP("Language", "Konkani") },
	{ "kos", QT_TRANSLATE_NOOP("Language", "Kosraean") },
	{ "kpe", QT_TRANSLATE_NOOP("Language", "Kpelle") },
	{ "kum", QT_TRANSLATE_NOOP("Language", "Kumyk") },
	{ "kru", QT_TRANSLATE_NOOP("Language", "Kurukh") },
	{ "kut", QT_TRANSLATE_NOOP("Language", "Kutenai") },
	{ "lad", QT_TRANSLATE_NOOP("Language", "Ladino") },
	{ "lah", QT_TRANSLATE_NOOP("Language", "Lahnda") },
	{ "lam", QT_TRANSLATE_NOOP("Language", "Lamba") },
	{ "lez", QT_TRANSLATE_NOOP("Language", "Lezghian") },
	{ "jbo", QT_TRANSLATE_NOOP("Language", "Lojban") },
	{ "nds", QT_TRANSLATE_NOOP("Language", "Low German") },
	{ "dsb", QT_TRANSLATE_NOOP("Language", "Lower Sorbian") },
	{ "loz", QT_TRANSLATE_NOOP("Language", "Lozi") },
	{ "lua", QT_TRANSLATE_NOOP("Language", "Luba-Lulua") },
	{ "lui", QT_TRANSLATE_NOOP("Language", "Luiseno") },
	{ "smj", QT_TRANSLATE_NOOP("Language", "Lule Sami") },
	{ "lun", QT_TRANSLATE_NOOP("Language", "Lunda") },
	{ "luo", QT_TRANSLATE_NOOP("Language", "Luo") },
	{ "lus", QT_TRANSLATE_NOOP("Language", "Lushai") },
	{ "mad", QT_TRANSLATE_NOOP("Language", "Madurese") },
	{ "mag", QT_TRANSLATE_NOOP("Language", "Magahi") },
	{ "mai", QT_TRANSLATE_NOOP("Language", "Maithili") },
	{ "mak", QT_TRANSLATE_NOOP("Language", "Makasar") },
	{ "mnc", QT_TRANSLATE_NOOP("Language", "Manchu") },
	{ "mdr", QT_TRANSLATE_NOOP("Language", "Mandar") },
	{ "man", QT_TRANSLATE_NOOP("Language", "Mandingo") },
	{ "mni", QT_TRANSLATE_NOOP("Language", "Manipuri") },
	{ "arn", QT_TRANSLATE_NOOP("Language", "Mapudungun") },
	{ "chm", QT_TRANSLATE_NOOP("Language", "Mari") },
	{ "mwr", QT_TRANSLATE_NOOP("Language", "Marwari") },
	{ "mas", QT_TRANSLATE_NOOP("Language", "Masai") },
	{ "men", QT_TRANSLATE_NOOP("Language", "Mende") },
	{ "mic", QT_TRANSLATE_NOOP("Language", "Micmac") },
	{ "dum", QT_TRANSLATE_NOOP("Language", "Middle Dutch") },
	{ "enm", QT_TRANSLATE_NOOP("Language", "Middle English") },
	{ "frm", QT_TRANSLATE_NOOP("Language", "Middle French") },
	{ "gmh", QT_TRANSLATE_NOOP("Language", "Middle High German") },
	{ "mga", QT_TRANSLATE_NOOP("Language", "Middle Irish") },
	{ "min", QT_TRANSLATE_NOOP("Language", "Minangkabau") },
	{ "mwl", QT_TRANSLATE_NOOP("Language", "Mirandese") },
	{ "moh", QT_TRANSLATE_NOOP("Language", "Mohawk") },
	{ "mdf", QT_TRANSLATE_NOOP("Language", "Moksha") },
	{ "lol", QT_TRANSLATE_NOOP("Language", "Mongo") },
	{ "cnr", QT_TRANSLATE_NOOP("Language", "Montenegrin") },
	{ "mos", QT_TRANSLATE_NOOP("Language", "Mossi") },
	{ "mul", QT_TRANSLATE_NOOP("Language", "Multiple languages") },
	{ "nqo", QT_TRANSLATE_NOOP("Language", "N'Ko") },
	{ "nap", QT_TRANSLATE_NOOP("Language", "Neapolitan") },
	{ "new", QT_TRANSLATE_NOOP("Language", "Newari") },
	{ "nia", QT_TRANSLATE_NOOP("Language", "Nias") },
	{ "niu", QT_TRANSLATE_NOOP("Language", "Niuean") },
	{ "nog", QT_TRANSLATE_NOOP("Language", "Nogai") },
	{ "frr", QT_TRANSLATE_NOOP("Language", "Northern Frisian") },
	{ "nso", QT_TRANSLATE_NOOP("Language", "Northern Sotho") },
	{ "nym", QT_TRANSLATE_NOOP("Language", "Nyamwezi") },
	{ "nyn", QT_TRANSLATE_NOOP("Language", "Nyankole") },
	{ "nyo", QT_TRANSLATE_NOOP("Language", "Nyoro") },
	{ "nzi", QT_TRANSLATE_NOOP("Language", "Nzima") },
	{ "ang", QT_TRANSLATE_NOOP("Language", "Old English") },
	{ "fro", QT_TRANSLATE_NOOP("Language", "Old French") },
	{ "goh", QT_TRANSLATE_NOOP("Language", "Old High German") },
	{ "sga", QT_TRANSLATE_NOOP("Language", "Old Irish") },
	{ "nwc", QT_TRANSLATE_NOOP("Language", "Old Newari") },
	{ "non", QT_TRANSLATE_NOOP("Language", "Old Norse") },
	{ "pro", QT_TRANSLATE_NOOP("Language", "Old Occitan") },
	{ "peo", QT_TRANSLATE_NOOP("Language", "Old Persian") },
	{ "osa", QT_TRANSLATE_NOOP("Language", "Osage") },
	{ "ota", QT_TRANSLATE_NOOP("Language", "Ottoman Turkish") },
	{ "pal", QT_TRANSLATE_NOOP("Language", "Pahlavi") },
	{ "pau", QT_TRANSLATE_NOOP("Language", "Palauan") },
	{ "pam", QT_TRANSLATE_NOOP("Language", "Pampanga") },
	{ "pag", QT_TRANSLATE_NOOP("Language", "Pangasinan") },
	{ "pap", QT_TRANSLATE_NOOP("Language", "Papiamento") },
	{ "phn", QT_TRANSLATE_NOOP("Language", "Phoenician") },
	{ "pon", QT_TRANSLATE_NOOP("Language", "Pohnpeian") },
	{ "raj", QT_TRANSLATE_NOOP("Language", "Rajasthani") },
	{ "rap", QT_TRANSLATE_NOOP("Language", "Rapanui") },
	{ "rar", QT_TRANSLATE_NOOP("Language", "Rarotongan") },
	{ "rom", QT_TRANSLATE_NOOP("Language", "Romany") },
	{ "sam", QT_TRANSLATE_NOOP("Language", "Samaritan Aramaic") },
	{ "sad", QT_TRANSLATE_NOOP("Language", "Sandawe") },
	{ "sat", QT_TRANSLATE_NOOP("Language", "Santali") },
	{ "sas", QT_TRANSLATE_NOOP("Language", "Sasak") },
	{ "sco", QT_TRANSLATE_NOOP("Language", "Scots") },
	{ "sel", QT_TRANSLATE_NOOP("Language", "Selkup") },
	{ "srr", QT_TRANSLATE_NOOP("Language", "Serer") },
	{ "shn", QT_TRANSLATE_NOOP("Language", "Shan") },
	{ "scn", QT_TRANSLATE_NOOP("Language", "Sicilian") },
	{ "sid", QT_TRANSLATE_NOOP("Language", "Sidamo") },
	{ "bla", QT_TRANSLATE_NOOP("Language", "Siksika") },
	{ "sms", QT_TRANSLATE_NOOP("Language", "Skolt Sami") },
	{ "sog", QT_TRANSLATE_NOOP("Language", "Sogdian") },
	{ "snk", QT_TRANSLATE_NOOP("Language", "Soninke") },
	{ "alt", QT_TRANSLATE_NOOP("Language", "Southern Altai") },
	{ "sma", QT_TRANSLATE_NOOP("Language", "Southern Sami") },
	{ "srn", QT_TRANSLATE_NOOP("Language", "Sranan Tongo") },
	{ "zgh", QT_TRANSLATE_NOOP("Language", "Standard Moroccan Tamazight") },
	{ "suk", QT_TRANSLATE_NOOP("Language", "Sukuma") },
	{ "sux", QT_TRANSLATE_NOOP("Language", "Sumerian") },
	{ "sus", QT_TRANSLATE_NOOP("Language", "Susu") },
	{ "gsw", QT_TRANSLATE_NOOP("Language", "Swiss German") },
	{ "syr", QT_TRANSLATE_NOOP("Language", "Syriac") },
	{ "tmh", QT_TRANSLATE_NOOP("Language", "Tamashek") },
	{ "ter", QT_TRANSLATE_NOOP("Language", "Tereno") },
	{ "tet", QT_TRANSLATE_NOOP("Language", "Tetum") },
	{ "tig", QT_TRANSLATE_NOOP("Language", "Tigre") },
	{ "tem", QT_TRANSLATE_NOOP("Language", "Timne") },
	{ "tiv", QT_TRANSLATE_NOOP("Language", "Tiv") },
	{ "tli", QT_TRANSLATE_NOOP("Language", "Tlingit") },
	{ "tpi", QT_TRANSLATE_NOOP("Language", "Tok Pisin") },
	{ "tkl", QT_TRANSLATE_NOOP("Language", "Tokelau") },
	{ "tog", QT_TRANSLATE_NOOP("Language", "Tonga (Nyasa)") },
	{ "tsi", QT_TRANSLATE_NOOP("Language", "Tsimshian") },
	{ "tum", QT_TRANSLATE_NOOP("Language", "Tumbuka") },
	{ "tvl", QT_TRANSLATE_NOOP("Language", "Tuvalu") },
	{ "tyv", QT_TRANSLATE_NOOP("Language", "Tuvinian") },
	{ "udm", QT_TRANSLATE_NOOP("Language", "Udmurt") },
	{ "uga", QT_TRANSLATE_NOOP("Language", "Ugaritic") },
	{ "umb", QT_TRANSLATE_NOOP("Language", "Umbundu") },
	{ "hsb", QT_TRANSLATE_NOOP("Language", "Upper Sorbian") },
	{ "vai", QT_TRANSLATE_NOOP("Language", "Vai") },
	{ "vot", QT_TRANSLATE_NOOP("Language", "Votic") },
	{ "war", QT_TRANSLATE_NOOP("Language", "Waray") },
	{ "was", QT_TRANSLATE_NOOP("Language", "Washo") },
	{ "wal", QT_TRANSLATE_NOOP("Language", "Wolaitta") },
	{ "sah", QT_TRANSLATE_NOOP("Language", "Yakut") },
	{ "yao", QT_TRANSLATE_NOOP("Language", "Yao") },
	{ "yap", QT_TRANSLATE_NOOP("Language", "Yapese") },
	{ "zap", QT_TRANSLATE_NOOP("Language", "Zapotec") },
	{ "zza", QT_TRANSLATE_NOOP("Language", "Zazaki") },
	{ "zen", QT_TRANSLATE_NOOP("Language", "Zenaga") },
	{ "zun", QT_TRANSLATE_NOOP("Language", "Zuni") },
	{ "rue", QT_TRANSLATE_NOOP("Language", "Rusinian") },
	{ "brh", QT_TRANSLATE_NOOP("Language", "Brauhi") },
	{ "evn", QT_TRANSLATE_NOOP("Language", "Evenki") },
};

inline std::unordered_map<QString, const char*> GetLanguagesMap()
{
	std::unordered_map<QString, const char*> result;
	std::ranges::transform(LANGUAGES, std::inserter(result, result.end()), [](const auto& item) {
		return std::make_pair(QString { item.key }, item.title);
	});
	return result;
}

} // namespace HomeCompa

#define TR_DEF                         \
	inline QString Tr(const char* str) \
	{                                  \
		return Loc::Tr(CONTEXT, str);  \
	}
