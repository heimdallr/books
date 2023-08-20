#pragma once

namespace HomeCompa::Flibrary {

#if(false)
QT_TRANSLATE_NOOP("ViewSource", "List")
QT_TRANSLATE_NOOP("ViewSource", "Tree")
#endif

enum class BooksViewType
{
	Undefined = -1,
	List,
	Tree,
};

[[maybe_unused]] constexpr std::pair<BooksViewType, const char *> g_viewSourceBooksModelItems[]
{
	{ BooksViewType::List, "List" },
	{ BooksViewType::Tree, "Tree" },
};

}
