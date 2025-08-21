#include "CollectionAutoUpdater.h"

#include "fnd/observable.h"

#include "log.h"

using namespace HomeCompa::Flibrary;

class CollectionAutoUpdater::Impl final : public Observable<IObserver>
{
};

CollectionAutoUpdater::CollectionAutoUpdater()
{
	PLOGV << "CollectionAutoUpdater created";
}

CollectionAutoUpdater::~CollectionAutoUpdater()
{
	PLOGV << "CollectionAutoUpdater destroyed";
}

void CollectionAutoUpdater::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void CollectionAutoUpdater::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
