#pragma once

#include <QString>

#include "export/util.h"

namespace HomeCompa::Util
{

UTIL_EXPORT QString md5(const QByteArray& data);
UTIL_EXPORT QString GetSaltedHash(const QString& auth);
UTIL_EXPORT QString GetSaltedHash(const QString& user, const QString& password);

}
