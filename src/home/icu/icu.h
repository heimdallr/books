#pragma once

#include <string>

#include "export/icu.h"

namespace HomeCompa::ICU
{
static constexpr auto LIB_NAME = "icu";
static constexpr auto TRANSLITERATE_NAME = "ICU_Transliterate";
using TransliterateType = void (*)(const char* id, const std::u32string* src, std::u32string* dst);
}

extern "C" ICU_EXPORT void ICU_Transliterate(const char* id, const std::u32string* src, std::u32string* dst);
