#pragma once

#include "export/flint.h"

class QString;

namespace HomeCompa::Flibrary {

struct FLINT_EXPORT QStringWrapper
{
	const QString & data;

	bool operator<(const QStringWrapper& rhs) const;
	static void SetLocale(const QString& locale);
};

}
