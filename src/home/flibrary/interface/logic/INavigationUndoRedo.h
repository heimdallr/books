#pragma once

#include "fnd/observer.h"

#include "interface/constants/Enums.h"

class QString;

namespace HomeCompa::Flibrary
{

class INavigationUndoRedo // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IObserver : public Observer
	{
	public:
		virtual void OnStateChanged()                                                                      = 0;
		virtual void OnApply(NavigationMode navigationMode, const QString& navigationId, long long bookId) = 0;
	};

public:
	virtual ~INavigationUndoRedo() = default;

	virtual void Undo() = 0;
	virtual void Redo() = 0;

	virtual bool IsUndoAvailable() const noexcept = 0;
	virtual bool IsRedoAvailable() const noexcept = 0;

	virtual void SetCurrentNavigation(NavigationMode navigationMode, QString navigationId) = 0;
	virtual void SetCurrentBook(long long bookId)                                          = 0;

	virtual void RegisterObserver(IObserver* observer)   = 0;
	virtual void UnregisterObserver(IObserver* observer) = 0;
};

} // namespace HomeCompa::Flibrary
