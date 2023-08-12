#pragma once

#include "UtilLib.h"

namespace HomeCompa {
class ISettings;
}

class QFont;

namespace HomeCompa::Util {

UTIL_API void Serialize(const QFont & font, ISettings & settings);
UTIL_API void Deserialize(QFont & font, const ISettings & settings);

}
