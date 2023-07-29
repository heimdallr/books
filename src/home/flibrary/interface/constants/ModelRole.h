#pragma once

#include <qnamespace.h>

namespace HomeCompa::Flibrary {

struct Role
{
	enum Value
	{
		Id = Qt::UserRole + 1,
		Type,
		MappedColumn,

		// global
		Filter,
		Checkable,
	};
};

}
