#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/INavigationUndoRedo.h"

namespace HomeCompa::Flibrary
{

class NavigationUndoRedo final : public INavigationUndoRedo
{
	NON_COPY_MOVABLE(NavigationUndoRedo)

public:
	NavigationUndoRedo();
	~NavigationUndoRedo() override;

private:
	void Undo() override;
	void Redo() override;

	bool IsUndoAvailable() const noexcept override;
	bool IsRedoAvailable() const noexcept override;

	void SetCurrentNavigation(NavigationMode navigationMode, QString navigationId) override;
	void SetCurrentBook(long long bookId) override;

	void RegisterObserver(IObserver* observer) override;
	void UnregisterObserver(IObserver* observer) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
