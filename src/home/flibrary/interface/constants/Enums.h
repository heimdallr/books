﻿#pragma once

namespace HomeCompa::Flibrary
{

#define NAVIGATION_MODE_ITEMS_X_MACRO \
	NAVIGATION_MODE_ITEM(Authors)     \
	NAVIGATION_MODE_ITEM(Series)      \
	NAVIGATION_MODE_ITEM(Genres)      \
	NAVIGATION_MODE_ITEM(Keywords)    \
	NAVIGATION_MODE_ITEM(Updates)     \
	NAVIGATION_MODE_ITEM(Archives)    \
	NAVIGATION_MODE_ITEM(Languages)   \
	NAVIGATION_MODE_ITEM(Groups)      \
	NAVIGATION_MODE_ITEM(Search)      \
	NAVIGATION_MODE_ITEM(AllBooks)

enum class NavigationMode
{
	Unknown = -1,
#define NAVIGATION_MODE_ITEM(NAME) NAME,
	NAVIGATION_MODE_ITEMS_X_MACRO
#undef NAVIGATION_MODE_ITEM
		Last
};

enum class ViewMode
{
	Unknown = -1,
	List,
	Tree,
	Last
};

enum class ItemType
{
	Unknown = -1,
	Navigation,
	Books,
	Last
};

#define BOOKS_MENU_ACTION_ITEMS_X_MACRO           \
	BOOKS_MENU_ACTION_ITEM(ReadBook)              \
	BOOKS_MENU_ACTION_ITEM(RemoveBook)            \
	BOOKS_MENU_ACTION_ITEM(RemoveBookFromArchive) \
	BOOKS_MENU_ACTION_ITEM(UndoRemoveBook)        \
	BOOKS_MENU_ACTION_ITEM(AddToNewGroup)         \
	BOOKS_MENU_ACTION_ITEM(AddToGroup)            \
	BOOKS_MENU_ACTION_ITEM(RemoveFromGroup)       \
	BOOKS_MENU_ACTION_ITEM(RemoveFromAllGroups)   \
	BOOKS_MENU_ACTION_ITEM(SetUserRate)           \
	BOOKS_MENU_ACTION_ITEM(CheckAll)              \
	BOOKS_MENU_ACTION_ITEM(UncheckAll)            \
	BOOKS_MENU_ACTION_ITEM(InvertCheck)           \
	BOOKS_MENU_ACTION_ITEM(Collapse)              \
	BOOKS_MENU_ACTION_ITEM(Expand)                \
	BOOKS_MENU_ACTION_ITEM(CollapseAll)           \
	BOOKS_MENU_ACTION_ITEM(ExpandAll)             \
	BOOKS_MENU_ACTION_ITEM(SendAsArchive)         \
	BOOKS_MENU_ACTION_ITEM(SendAsIs)              \
	BOOKS_MENU_ACTION_ITEM(SendUnpack)            \
	BOOKS_MENU_ACTION_ITEM(SendAsInpxCollection)  \
	BOOKS_MENU_ACTION_ITEM(SendAsInpxFile)        \
	BOOKS_MENU_ACTION_ITEM(SendAsScript)

enum class BooksMenuAction
{
	None = -1,
#define BOOKS_MENU_ACTION_ITEM(NAME) NAME,
	BOOKS_MENU_ACTION_ITEMS_X_MACRO
#undef BOOKS_MENU_ACTION_ITEM
		Last
};

} // namespace HomeCompa::Flibrary
