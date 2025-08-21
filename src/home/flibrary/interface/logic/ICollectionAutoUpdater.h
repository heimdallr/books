#pragma once

#include "fnd/observer.h"

namespace HomeCompa::Flibrary
{

class ICollectionAutoUpdater // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IObserver : public Observer
	{
	public:
		virtual void OnCollectionUpdated() = 0;
	};

public:
	virtual ~ICollectionAutoUpdater() = default;

	virtual void RegisterObserver(IObserver* observer) = 0;
	virtual void UnregisterObserver(IObserver* observer) = 0;
};

}
