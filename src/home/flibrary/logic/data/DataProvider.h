#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

class QString;

namespace HomeCompa::Flibrary {

class DataProvider
{
	NON_COPY_MOVABLE(DataProvider)

public:

public:
	explicit DataProvider(std::shared_ptr<class ILogicFactory> logicFactory);
	~DataProvider();

public:
	void SetNavigationMode(enum class NavigationMode navigationMode);
	void SetViewMode(enum class ViewMode viewMode);

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
