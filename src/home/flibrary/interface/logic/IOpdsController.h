#pragma once

#include <qnamespace.h>

#include "fnd/observer.h"

namespace HomeCompa::Flibrary
{

class IOpdsController // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	static constexpr auto        CONTEXT = "OpdsDialog";
	static constexpr const char* ON_APP_EXIT[] {
		QT_TRANSLATE_NOOP("OpdsDialog", "Continue working"),
		QT_TRANSLATE_NOOP("OpdsDialog", "Ask every time"),
		QT_TRANSLATE_NOOP("OpdsDialog", "Shutdown server"),
	};

public:
	class IObserver : public Observer
	{
	public:
		virtual void OnRunningChanged() = 0;
	};

public:
	virtual ~IOpdsController() = default;

public:
	virtual bool IsRunning() const = 0;
	virtual void Start()           = 0;
	virtual void Stop()            = 0;
	virtual void Restart()         = 0;

	virtual bool InStartup() const         = 0;
	virtual void AddToStartup() const      = 0;
	virtual void RemoveFromStartup() const = 0;

	virtual void RegisterObserver(IObserver* observer)   = 0;
	virtual void UnregisterObserver(IObserver* observer) = 0;
};

} // namespace HomeCompa::Flibrary
