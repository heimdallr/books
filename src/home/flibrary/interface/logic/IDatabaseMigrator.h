#pragma once

#include "fnd/observer.h"

namespace HomeCompa::Flibrary
{

class IDatabaseMigrator // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IObserver : public Observer
	{
	public:
		virtual void OnMigrationFinished() = 0;
	};

public:
	virtual ~IDatabaseMigrator() = default;
	virtual bool NeedMigrate() const = 0;
	virtual void Migrate() = 0;

	virtual void RegisterObserver(IObserver* observer) = 0;
	virtual void UnregisterObserver(IObserver* observer) = 0;
};

}
