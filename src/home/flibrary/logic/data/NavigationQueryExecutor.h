#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "interface/logic/INavigationQueryExecutor.h"

namespace HomeCompa::Flibrary {

class NavigationQueryExecutor final : virtual public INavigationQueryExecutor
{
	NON_COPY_MOVABLE(NavigationQueryExecutor)

public:
	explicit NavigationQueryExecutor(std::shared_ptr<class IDatabaseUser> databaseUser);
	~NavigationQueryExecutor() override;

private: // INavigationQueryExecutor
	void RequestNavigation(NavigationMode navigationMode, Callback callback) const override;
	const QueryDescription & GetQueryDescription(NavigationMode navigationMode) const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
