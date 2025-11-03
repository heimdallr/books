#pragma once

#include <QString>

#include "export/util.h"

namespace HomeCompa::Util
{

UTIL_EXPORT QStringList ResolveWildcard(const QString& wildcard);
UTIL_EXPORT QString     RemoveIllegalPathCharacters(QString str);
UTIL_EXPORT QString     ToRelativePath(const QString& path);
UTIL_EXPORT QString     ToAbsolutePath(const QString& path);

}
