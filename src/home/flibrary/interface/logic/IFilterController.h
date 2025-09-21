#pragma once

#include "IDataItem.h"
#include "IFilterProvider.h"

class QString;

namespace HomeCompa::Flibrary
{
enum class NavigationMode;

class IFilterController : public IFilterProvider
{
public:
	virtual void SetFilterEnabled(bool enabled) = 0;
	virtual void SetNavigationItemFlags(NavigationMode navigationMode, const QString& navigationId, IDataItem::Flags flags) = 0;
};

} // namespace HomeCompa::Flibrary
