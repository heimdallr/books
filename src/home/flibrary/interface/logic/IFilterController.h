#pragma once

#include <QString>

#include "database/interface/ICommand.h"
#include "database/interface/IQuery.h"

#include "interface/constants/Enums.h"

#include "IDataItem.h"
#include "IFilterProvider.h"
#include "IModelProvider.h"

#include "export/flint.h"

namespace HomeCompa::Flibrary
{

class IFilterController : public IFilterProvider
{
public:
	using Callback = std::function<void()>;
	using CommandBinder = void (*)(DB::ICommand& command, size_t index, const QString& value);
	using QueueBinder = void (*)(DB::IQuery& command, size_t index, const QString& value);
	using ModelGetter = std::shared_ptr<QAbstractItemModel> (IModelProvider::*)(IDataItem::Ptr data) const;

	struct FilteredNavigation
	{
		NavigationMode navigationMode { NavigationMode::Unknown };
		const char* navigationTitle { nullptr };
		ModelGetter modelGetter { nullptr };
		const char* table { nullptr };
		const char* idField { nullptr };
		CommandBinder commandBinder { nullptr };
		QueueBinder queueBinder { nullptr };
	};

public:
	FLINT_EXPORT static const FilteredNavigation& GetFilteredNavigationDescription(size_t index);

public:
	virtual void SetFilterEnabled(bool enabled) = 0;
	virtual void SetNavigationItemFlags(NavigationMode navigationMode, QStringList navigationIds, IDataItem::Flags flags, Callback callback) = 0;
	virtual void ClearNavigationItemFlags(NavigationMode navigationMode, QStringList navigationIds, IDataItem::Flags flags, Callback callback) = 0;
};

} // namespace HomeCompa::Flibrary
