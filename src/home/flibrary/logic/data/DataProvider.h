#pragma once

#include <functional>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "Types.h"

class QString;

namespace HomeCompa::Flibrary {

class DataProvider
{
	NON_COPY_MOVABLE(DataProvider)

public:
	using Callback = std::function<void(DataItem::Ptr)>;

public:
	explicit DataProvider(std::shared_ptr<class ILogicFactory> logicFactory);
	~DataProvider();

public:
	void SetNavigationMode(enum class NavigationMode navigationMode);
	void SetViewMode(enum class ViewMode viewMode);

	void RequestNavigation(Callback callback);

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
