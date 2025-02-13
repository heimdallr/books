#pragma once

#include <QString>

#include "export/util.h"

namespace HomeCompa::Util
{

UTIL_EXPORT QStringList ResolveWildcard(const QString& wildcard);

}
