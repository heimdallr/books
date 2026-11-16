#pragma once

#include <qnamespace.h>

#include <unordered_set>

#include <QModelIndex>

#include "fnd/algorithm.h"

#include "logic/data/DataItem.h"

namespace HomeCompa::Flibrary {

#define FONT_SETTING_SETTER_ITEMS_X_MACRO          \
	FONT_SETTING_SETTER_ITEM(setBold)              \
	FONT_SETTING_SETTER_ITEM(setCapitalization)    \
	FONT_SETTING_SETTER_ITEM(setFamily)            \
	FONT_SETTING_SETTER_ITEM(setFixedPitch)        \
	FONT_SETTING_SETTER_ITEM(setHintingPreference) \
	FONT_SETTING_SETTER_ITEM(setItalic)            \
	FONT_SETTING_SETTER_ITEM(setKerning)           \
	FONT_SETTING_SETTER_ITEM(setOverline)          \
	FONT_SETTING_SETTER_ITEM(setPixelSize)         \
	FONT_SETTING_SETTER_ITEM(setPointSize)         \
	FONT_SETTING_SETTER_ITEM(setStretch)           \
	FONT_SETTING_SETTER_ITEM(setStrikeOut)         \
	FONT_SETTING_SETTER_ITEM(setStyle)             \
	FONT_SETTING_SETTER_ITEM(setStyleName)         \
	FONT_SETTING_SETTER_ITEM(setStyleStrategy)     \
	FONT_SETTING_SETTER_ITEM(setUnderline)         \
	FONT_SETTING_SETTER_ITEM(setWeight)            \
	FONT_SETTING_SETTER_ITEM(setWordSpacing)

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
		ColumnCount,
		CheckableColumn,
		TextFilter,
		ShowRemovedFilter,
		ShowAlreadyReadFilter,
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
		SetForIds,
		RemovedItemBackgroundColor,
		RemovedItemForegroundColor,
#define FONT_SETTING_SETTER_ITEM(NAME) RemovedItemFont_##NAME,
		FONT_SETTING_SETTER_ITEMS_X_MACRO
#undef FONT_SETTING_SETTER_ITEM
			Last
	};

	inline static constexpr int FirstItemColumn = Author;
};

struct SelectedRequest
{
	QModelIndex      current;
	QModelIndexList  selected;
	QModelIndexList* result { nullptr };
};

struct SetForIdsArg
{
	std::unordered_set<long long> ids;
	int                           column;
	QString                       value;
};

using FastFilterItems = std::unordered_set<QVariant, Util::VariantHash>;

} // namespace HomeCompa::Flibrary

Q_DECLARE_METATYPE(HomeCompa::Flibrary::SelectedRequest)
Q_DECLARE_METATYPE(HomeCompa::Flibrary::SetForIdsArg)
Q_DECLARE_METATYPE(HomeCompa::Flibrary::FastFilterItems*)
Q_DECLARE_METATYPE(const HomeCompa::Flibrary::FastFilterItems*)
