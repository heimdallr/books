#pragma once

#include <utility>

namespace HomeCompa::Flibrary {

#if(false)
QT_TRANSLATE_NOOP("ViewSource", "Authors")
QT_TRANSLATE_NOOP("ViewSource", "Series")
QT_TRANSLATE_NOOP("ViewSource", "Genres")
#endif
namespace Constant {
[[maybe_unused]] constexpr auto NavigationSourceTranslationContext = "ViewSource";
}

#define VIEW_SOURCE_NAVIGATION_MODEL_ITEMS_XMACRO  \
		VIEW_SOURCE_NAVIGATION_MODEL_ITEM(Authors) \
		VIEW_SOURCE_NAVIGATION_MODEL_ITEM(Series)  \
		VIEW_SOURCE_NAVIGATION_MODEL_ITEM(Genres)  \

enum class NavigationSource
{
	Undefined = -1,
#define VIEW_SOURCE_NAVIGATION_MODEL_ITEM(NAME) NAME,
	VIEW_SOURCE_NAVIGATION_MODEL_ITEMS_XMACRO
#undef	VIEW_SOURCE_NAVIGATION_MODEL_ITEM
};

[[maybe_unused]] constexpr std::pair<NavigationSource, const char *> g_viewSourceNavigationModelItems[]
{
#define VIEW_SOURCE_NAVIGATION_MODEL_ITEM(NAME) { NavigationSource::NAME, #NAME },
		VIEW_SOURCE_NAVIGATION_MODEL_ITEMS_XMACRO
#undef	VIEW_SOURCE_NAVIGATION_MODEL_ITEM
};

}
