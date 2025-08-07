#include "translit.h"

#include <QString>

namespace HomeCompa::Util
{

namespace
{

void Transliterate(const ICU::TransliterateType transliterate, const char* id, QString& str)
{
	assert(transliterate);
	auto src = str.toStdU32String();
	src.push_back(0);
	std::u32string dst;
	transliterate(id, &src, &dst);
	str = QString::fromStdU32String(dst);
}

}

QString Transliterate(const ICU::TransliterateType transliterate, QString fileName)
{
	if (!transliterate)
		return fileName;

	Transliterate(transliterate, "ru-ru_Latn/BGN", fileName);
	Transliterate(transliterate, "Any-Latin", fileName);
	Transliterate(transliterate, "Latin-ASCII", fileName);

	return fileName.replace(' ', '_');
}

} // namespace HomeCompa::Util
