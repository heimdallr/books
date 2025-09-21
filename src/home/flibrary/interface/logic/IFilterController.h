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
	using Callback = std::function<void()>;

public:
	virtual void SetFilterEnabled(bool enabled) = 0;
	virtual void SetNavigationItemFlags(NavigationMode navigationMode, QStringList navigationIds, IDataItem::Flags flags, Callback callback) = 0;
	virtual void ClearNavigationItemFlags(NavigationMode navigationMode, QStringList navigationIds, IDataItem::Flags flags, Callback callback) = 0;
};

} // namespace HomeCompa::Flibrary
