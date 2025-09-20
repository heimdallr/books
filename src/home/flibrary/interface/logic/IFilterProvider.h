#pragma once

#include "fnd/observer.h"

class QString;

namespace HomeCompa::Flibrary
{

class IFilterProvider // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IObserver : public Observer
	{
	public:
		virtual void OnFilterEnabledChanged() = 0;
	};

public:
	virtual ~IFilterProvider() = default;

	[[nodiscard]] virtual bool IsFilterEnabled() const noexcept = 0;

	virtual void RegisterObserver(IObserver* observer) = 0;
	virtual void UnregisterObserver(IObserver* observer) = 0;
};

class IFilterController : public IFilterProvider
{
public:
	virtual void SetFilterEnabled(bool enabled) = 0;
};

} // namespace HomeCompa::Flibrary
