#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/ICollectionProvider.h"
#include "interface/logic/IDatabaseUser.h"
#include "interface/logic/INavigationQueryExecutor.h"

namespace HomeCompa::Flibrary
{

class NavigationQueryExecutor final : virtual public INavigationQueryExecutor
{
	NON_COPY_MOVABLE(NavigationQueryExecutor)

public:
	NavigationQueryExecutor(std::shared_ptr<IDatabaseUser> databaseUser, std::shared_ptr<const ICollectionProvider> collectionProvider);
	~NavigationQueryExecutor() override;

private: // INavigationQueryExecutor
	void RequestNavigation(NavigationMode navigationMode, Callback callback, bool force) const override;
	const QueryDescription& GetQueryDescription(NavigationMode navigationMode) const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
