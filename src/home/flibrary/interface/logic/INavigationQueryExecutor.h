#pragma once

#include <functional>

#include "IDataItem.h"

namespace HomeCompa::DB {
class IDatabase;
}

namespace HomeCompa::Flibrary {

struct QueryDescription;
enum class NavigationMode;

class INavigationQueryExecutor // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using Callback = std::function<void(NavigationMode mode, IDataItem::Ptr root)>;

public:
	virtual ~INavigationQueryExecutor() = default;
	virtual void RequestNavigation(NavigationMode navigationMode, Callback callback, bool force = false) const = 0;
	virtual const QueryDescription & GetQueryDescription(NavigationMode navigationMode) const = 0;
};

}
