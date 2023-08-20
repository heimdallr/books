#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "fnd/observer.h"

namespace HomeCompa::DB {
class IDatabase;
}

namespace HomeCompa::Flibrary {

class DatabaseController final
{
	NON_COPY_MOVABLE(DatabaseController)

public:
	class IObserver : public Observer
	{
	public:
		virtual void AfterDatabaseCreated(DB::IDatabase &) = 0;
		virtual void BeforeDatabaseDestroyed(DB::IDatabase &) = 0;
	};

public:
	explicit DatabaseController(std::shared_ptr<class ICollectionController> collectionController);
	~DatabaseController();

public:
	std::shared_ptr<DB::IDatabase> GetDatabase() const;

	void RegisterObserver(IObserver * observer);
	void UnregisterObserver(IObserver * observer);

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
