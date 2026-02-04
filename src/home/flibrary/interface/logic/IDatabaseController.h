#pragma once

#include "fnd/observer.h"

namespace HomeCompa::DB
{

class IDatabase;

}

namespace HomeCompa::Flibrary
{

class IDatabaseController // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IObserver : public Observer
	{
	public:
		virtual void AfterDatabaseCreated(DB::IDatabase&)    = 0;
		virtual void BeforeDatabaseDestroyed(DB::IDatabase&) = 0;
	};

public:
	virtual ~IDatabaseController() = default;

public:
	virtual std::shared_ptr<DB::IDatabase> GetDatabase(bool readOnly = false) const = 0;
	virtual std::shared_ptr<DB::IDatabase> CheckDatabase() const                    = 0;

	virtual void RegisterObserver(IObserver* observer)   = 0;
	virtual void UnregisterObserver(IObserver* observer) = 0;
};

} // namespace HomeCompa::Flibrary
