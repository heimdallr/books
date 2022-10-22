#pragma once

#include <QString>

#include "NavigationTreeItemDecl.h"

namespace HomeCompa::Flibrary {

struct NavigationTreeItem
{
	QString Id;
	QString Title;
	QString ParentId;
};

}
