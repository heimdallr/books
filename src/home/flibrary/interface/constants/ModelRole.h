#pragma once

#include <qnamespace.h>

#include "logic/data/DataItem.h"

namespace HomeCompa::Flibrary {

struct Role
{
	enum Value
	{
		Id = Qt::UserRole + 1,
		Type,
		CheckState,

#define	BOOKS_COLUMN_ITEM(NAME) NAME,
		BOOKS_COLUMN_ITEMS_X_MACRO
#undef	BOOKS_COLUMN_ITEM

		// global
		Filter,
		Checkable,
		Languages,
		LanguageFilter,
	};
};

}
