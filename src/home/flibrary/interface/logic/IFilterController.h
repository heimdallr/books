#pragma once

#include "interface/logic/IDataItem.h"
#include "interface/logic/IFilterProvider.h"

namespace HomeCompa::Flibrary
{

class IFilterController : public IFilterProvider
{
public:
	using Callback = std::function<void()>;

public:
	virtual void SetFilterEnabled(bool enabled) = 0;
	virtual void Apply() = 0;
	virtual void SetFlags(NavigationMode navigationMode, QString id, IDataItem::Flags flags) = 0;
	virtual void SetNavigationItemFlags(NavigationMode navigationMode, QStringList navigationIds, IDataItem::Flags flags, Callback callback) = 0;
	virtual void ClearNavigationItemFlags(NavigationMode navigationMode, QStringList navigationIds, IDataItem::Flags flags, Callback callback) = 0;
};

} // namespace HomeCompa::Flibrary
