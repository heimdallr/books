#pragma once

#include <qnamespace.h>

#include <unordered_set>

#include <QModelIndex>

#include "fnd/algorithm.h"

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
#define BOOKS_COLUMN_ITEM(NAME) NAME##Filter,
			BOOKS_COLUMN_ITEMS_X_MACRO
#undef BOOKS_COLUMN_ITEM
#define BOOKS_COLUMN_ITEM(NAME) NAME##sAll,
				BOOKS_COLUMN_ITEMS_X_MACRO
#undef BOOKS_COLUMN_ITEM
					Count,
		ChildCount,
		CheckableColumn,
		TextFilter,
		ShowRemovedFilter,
		NavigationItemFiltered,
		UniFilterEnabled,
		UniFilterChanged,
		UniFilterHideUnrated,
		UniFilterMinimumRate,
		UniFilterMaximumRate,
		VisibleColumns,
		Selected,
		SortOrder,
		Check,
		Uncheck,
		CheckAll,
		UncheckAll,
		InvertCheck,
		IsTree,
		HeaderName,
		HeaderTitle,
		Remap,
		NavigationMode,
		HideFiltered,
		HideFilteredCallback,
		ModelSorter,
		Last
	};
};

struct SelectedRequest
{
	QModelIndex      current;
	QModelIndexList  selected;
	QModelIndexList* result { nullptr };
};

using FastFilterItems = std::unordered_set<QVariant, Util::VariantHash>;

} // namespace HomeCompa::Flibrary

Q_DECLARE_METATYPE(HomeCompa::Flibrary::SelectedRequest)
Q_DECLARE_METATYPE(HomeCompa::Flibrary::FastFilterItems*)
Q_DECLARE_METATYPE(const HomeCompa::Flibrary::FastFilterItems*)
