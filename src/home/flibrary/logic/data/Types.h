#pragma once

namespace HomeCompa::Flibrary {

enum class NavigationMode
{
	Unknown = -1,
	Authors,
	Series,
	Genres,
	Archives,
	Groups,
	Last
};

enum class ViewMode
{
	Unknown = -1,
	List,
	Tree,
	Last
};

}
