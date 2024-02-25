#pragma once

#include <qnamespace.h>

#include "logic/data/DataItem.h"

#include <QModelIndex>

namespace HomeCompa::Flibrary {

struct Role
{
	enum Value
	{
		Id = Qt::UserRole + 1,
		Type,
		CheckState,
		IsRemoved,

#define	BOOKS_COLUMN_ITEM(NAME) NAME,
		BOOKS_COLUMN_ITEMS_X_MACRO
#undef	BOOKS_COLUMN_ITEM

		// global
		Count,
		Checkable,
		Languages,
		TextFilter,
		LanguageFilter,
		ShowRemovedFilter,
		Selected,
		SortOrder,
		CheckAll,
		UncheckAll,
		InvertCheck,
		IsTree,
		HeaderTitle,
	};
};

struct SelectedRequest
{
	QModelIndex current;
	QModelIndexList selected;
	QModelIndexList * result { nullptr };
};

}
