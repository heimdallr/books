#pragma once

#include "export/util.h"

class QString;

namespace HomeCompa::Util {

struct UTIL_EXPORT QStringWrapper
{
	const QString & data;

	bool operator<(const QStringWrapper& rhs) const;
	static void SetLocale(const QString& locale);
};

}
