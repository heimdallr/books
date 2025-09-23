#pragma once

#include <qnamespace.h>

#include <QModelIndex>

#include "logic/data/DataItem.h"

namespace HomeCompa::Flibrary
{

struct Role
{
	enum Value
	{
		Id = Qt::UserRole + 1,
		Type,
		CheckState,
		IsRemoved,
		Flags,

#define BOOKS_COLUMN_ITEM(NAME) NAME,
		BOOKS_COLUMN_ITEMS_X_MACRO
#undef BOOKS_COLUMN_ITEM

			// global
			Count,
		ChildCount,
		Checkable,
		Languages,
		TextFilter,
		LanguageFilter,
		ShowRemovedFilter,
		NavigationItemFiltered,
		UniFilterEnabled,
		UniFilterChanged,
		VisibleColumns,
		Selected,
		SortOrder,
		CheckAll,
		UncheckAll,
		InvertCheck,
		IsTree,
		HeaderName,
		HeaderTitle,
		Remap,
		NavigationMode,
		Last
	};
};

struct SelectedRequest
{
	QModelIndex current;
	QModelIndexList selected;
	QModelIndexList* result { nullptr };
};

}
