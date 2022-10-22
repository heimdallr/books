#pragma once

#include <QString>

#include "NavigationTreeItemDecl.h"

namespace HomeCompa::Flibrary {

struct NavigationTreeItem
{
	QString Id;
	QString Title;
	QString ParentId;
	int TreeLevel { 0 };
	int ChildrenCount { 0 };
	bool Expanded { false };
};

}
