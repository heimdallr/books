#pragma once

#include "export/util.h"

namespace HomeCompa
{
class ISettings;
}

class QFont;

namespace HomeCompa::Util
{

UTIL_EXPORT void Serialize(const QFont& font, ISettings& settings);
UTIL_EXPORT void Deserialize(QFont& font, const ISettings& settings);

}
