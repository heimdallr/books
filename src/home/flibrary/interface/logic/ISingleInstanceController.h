#pragma once

#include "fnd/observer.h"

namespace HomeCompa::Flibrary
{

class ISingleInstanceController // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IObserver : public Observer // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		virtual void OnStartAnotherApp() = 0;
	};

public:
	virtual ~ISingleInstanceController() = default;

	virtual bool IsFirstSingleInstanceApp() const = 0;

	virtual void RegisterObserver(IObserver* observer)   = 0;
	virtual void UnregisterObserver(IObserver* observer) = 0;
};

}
