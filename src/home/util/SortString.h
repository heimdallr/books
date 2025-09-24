#pragma once

#include <limits>

#include "export/util.h"

class QString;

namespace HomeCompa::Util
{

struct UTIL_EXPORT QStringWrapper
{
	const QString& data;

	static void SetLocale(const QString& locale);
	static bool Compare(const QStringWrapper& lhs, const QStringWrapper& rhs, int emptyStringWeight = std::numeric_limits<int>::max());

	bool operator<(const QStringWrapper& rhs) const;
};

}
