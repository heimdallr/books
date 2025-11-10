#pragma once

#include "export/util.h"

class QImage;

namespace HomeCompa::Util
{
UTIL_EXPORT QImage HasAlpha(const QImage& image, const char* data);
}
