#pragma once

namespace HomeCompa::Flibrary::Constant {

#if(false)
QT_TRANSLATE_NOOP("ViewSource", "Authors")
QT_TRANSLATE_NOOP("ViewSource", "Series")
QT_TRANSLATE_NOOP("ViewSource", "Genres")
#endif
[[maybe_unused]] constexpr auto ViewSourceNavigationModelItemContext = "ViewSource";

#define VIEW_SOURCE_NAVIGATION_MODEL_ITEMS_XMACRO  \
		VIEW_SOURCE_NAVIGATION_MODEL_ITEM(Authors) \
		VIEW_SOURCE_NAVIGATION_MODEL_ITEM(Series)  \
		VIEW_SOURCE_NAVIGATION_MODEL_ITEM(Genres)  \

enum class ViewSourceNavigationModelItem
{
#define VIEW_SOURCE_NAVIGATION_MODEL_ITEM(NAME) NAME,
		VIEW_SOURCE_NAVIGATION_MODEL_ITEMS_XMACRO
#undef	VIEW_SOURCE_NAVIGATION_MODEL_ITEM
};

[[maybe_unused]] constexpr std::pair<ViewSourceNavigationModelItem, const char *> g_viewSourceNavigationModelItems[]
{
#define VIEW_SOURCE_NAVIGATION_MODEL_ITEM(NAME) { ViewSourceNavigationModelItem::NAME, #NAME },
		VIEW_SOURCE_NAVIGATION_MODEL_ITEMS_XMACRO
#undef	VIEW_SOURCE_NAVIGATION_MODEL_ITEM
};

[[maybe_unused]] constexpr std::pair<const char *, const char *> g_viewSourceBooksModelItems[]
{
	{ "BooksListView", QT_TRANSLATE_NOOP("ViewSource", "List") },
	{ "BooksTreeView", QT_TRANSLATE_NOOP("ViewSource", "Tree") },
};

}
