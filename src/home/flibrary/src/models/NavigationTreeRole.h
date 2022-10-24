#pragma once

#include <QObject>

#include "RoleBase.h"

namespace HomeCompa::Flibrary {

#define NAVIGATION_TREE_ROLE_ITEMS_XMACRO        \
		NAVIGATION_TREE_ROLE_ITEM(TreeLevel)     \
		NAVIGATION_TREE_ROLE_ITEM(ChildrenCount) \
		NAVIGATION_TREE_ROLE_ITEM(Expanded)      \

struct NavigationTreeRole
	: RoleBase
{
	Q_OBJECT
public:
	enum Value
	{
		FakeNavigationTreeRoleFirst = FakeRoleLast + 1,
#define NAVIGATION_TREE_ROLE_ITEM(NAME) NAME,
		NAVIGATION_TREE_ROLE_ITEMS_XMACRO
#undef	NAVIGATION_TREE_ROLE_ITEM
		FakeNavigationTreeRoleLast
	};

	Q_ENUM(Value)
};

}
