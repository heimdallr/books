#pragma once

namespace HomeCompa::Flibrary
{

#define NAVIGATION_MODE_ITEMS_X_MACRO \
	NAVIGATION_MODE_ITEM(Authors)     \
	NAVIGATION_MODE_ITEM(Series)      \
	NAVIGATION_MODE_ITEM(Genres)      \
	NAVIGATION_MODE_ITEM(PublishYear) \
	NAVIGATION_MODE_ITEM(Keywords)    \
	NAVIGATION_MODE_ITEM(Updates)     \
	NAVIGATION_MODE_ITEM(Archives)    \
	NAVIGATION_MODE_ITEM(Languages)   \
	NAVIGATION_MODE_ITEM(Groups)      \
	NAVIGATION_MODE_ITEM(Search)      \
	NAVIGATION_MODE_ITEM(Reviews)     \
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

#define GROUPS_MENU_ACTION_ITEMS_X_MACRO \
	MENU_ACTION_ITEM(AddToNewGroup)      \
	MENU_ACTION_ITEM(AddToGroup)         \
	MENU_ACTION_ITEM(RemoveFromGroup)    \
	MENU_ACTION_ITEM(RemoveFromAllGroups)

constexpr int INVALID_MENU_ITEM = -1;

struct GroupsMenuAction
{
	enum
	{

#define MENU_ACTION_ITEM(NAME) NAME,
		GROUPS_MENU_ACTION_ITEMS_X_MACRO
#undef MENU_ACTION_ITEM
			Last
	};
};

#define BOOKS_MENU_ACTION_ITEMS_X_MACRO     \
	MENU_ACTION_ITEM(ReadBook)              \
	MENU_ACTION_ITEM(RemoveBook)            \
	MENU_ACTION_ITEM(RemoveBookFromArchive) \
	MENU_ACTION_ITEM(UndoRemoveBook)        \
	MENU_ACTION_ITEM(SetUserRate)           \
	MENU_ACTION_ITEM(CheckAll)              \
	MENU_ACTION_ITEM(UncheckAll)            \
	MENU_ACTION_ITEM(InvertCheck)           \
	MENU_ACTION_ITEM(Collapse)              \
	MENU_ACTION_ITEM(Expand)                \
	MENU_ACTION_ITEM(CollapseAll)           \
	MENU_ACTION_ITEM(ExpandAll)             \
	MENU_ACTION_ITEM(SendAsArchive)         \
	MENU_ACTION_ITEM(SendAsIs)              \
	MENU_ACTION_ITEM(SendUnpack)            \
	MENU_ACTION_ITEM(SendAsInpxCollection)  \
	MENU_ACTION_ITEM(SendAsInpxFile)        \
	MENU_ACTION_ITEM(SendAsScript)          \
	MENU_ACTION_ITEM(ChangeLanguage)

struct BooksMenuAction
{
	enum
	{
		Unknown = GroupsMenuAction::Last,

#define MENU_ACTION_ITEM(NAME) NAME,
		BOOKS_MENU_ACTION_ITEMS_X_MACRO
#undef MENU_ACTION_ITEM
			Last
	};
};

#define NAVIGATION_MENU_ACTION_ITEMS_X_MACRO     \
	MENU_ACTION_ITEM(CreateNewGroup)             \
	MENU_ACTION_ITEM(RenameGroup)                \
	MENU_ACTION_ITEM(RemoveGroup)                \
	MENU_ACTION_ITEM(CreateNewSearch)            \
	MENU_ACTION_ITEM(RemoveSearch)               \
	MENU_ACTION_ITEM(RemoveFromGroupOneItem)     \
	MENU_ACTION_ITEM(RemoveFromGroupAllBooks)    \
	MENU_ACTION_ITEM(RemoveFromGroupAllAuthors)  \
	MENU_ACTION_ITEM(RemoveFromGroupAllSeries)   \
	MENU_ACTION_ITEM(RemoveFromGroupAllKeywords) \
	MENU_ACTION_ITEM(AuthorReview)               \
	MENU_ACTION_ITEM(HideNavigationItem)         \
	MENU_ACTION_ITEM(FilterNavigationItemBooks)

struct MenuAction
{
	enum
	{
		Unknown = BooksMenuAction::Last,

#define MENU_ACTION_ITEM(NAME) NAME,
		NAVIGATION_MENU_ACTION_ITEMS_X_MACRO
#undef MENU_ACTION_ITEM
			Last
	};
};

} // namespace HomeCompa::Flibrary
