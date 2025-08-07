#pragma once

#include "icu/icu.h"

#include "export/util.h"

class QString;

namespace HomeCompa::Util
{

UTIL_EXPORT QString Transliterate(ICU::TransliterateType transliterate, QString fileName);

}
