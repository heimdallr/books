#include "icu.h"

#include <cassert>

#include <unicode/translit.h>

void ICU_Transliterate(const char* id, const std::u32string* src, std::u32string* dst)
{
	UErrorCode              status  = U_ZERO_ERROR;
	icu_77::Transliterator* myTrans = icu_77::Transliterator::createInstance(id, UTRANS_FORWARD, status);
	assert(myTrans);

	auto icuString = icu_77::UnicodeString::fromUTF32(reinterpret_cast<const int32_t*>(src->data()), -1);
	myTrans->transliterate(icuString);
	auto buf = std::make_unique_for_overwrite<int32_t[]>(icuString.length() * 4ULL);
	icuString.toUTF32(buf.get(), icuString.length() * 4, status);
	*dst = std::u32string { reinterpret_cast<char32_t*>(buf.get()) };
}
