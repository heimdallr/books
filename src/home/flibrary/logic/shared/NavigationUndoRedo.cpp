#include "NavigationUndoRedo.h"

#include <QTimer>

#include "fnd/algorithm.h"
#include "fnd/observable.h"

#include "settings/UiTimer.h"

using namespace HomeCompa::Flibrary;

namespace
{

struct Item
{
	NavigationMode navigationMode;
	QString        navigationId;
	long long      bookId;
};

using Stack = std::vector<Item>;

}

class NavigationUndoRedo::Impl final : public Observable<IObserver>
{
public:
	void Undo()
	{
		assert(IsUndoAvailable());
		--m_current;
		Apply();
	}

	void Redo()
	{
		assert(IsRedoAvailable());
		++m_current;
		Apply();
	}

	bool IsUndoAvailable() const noexcept
	{
		return m_current > 0;
	}

	bool IsRedoAvailable() const noexcept
	{
		return m_current + 1 < std::ssize(m_stack);
	}

	void SetCurrentNavigation(const NavigationMode navigationMode, QString navigationId)
	{
		m_navigationMode = navigationMode;
		m_navigationId   = std::move(navigationId);
	}

	void SetCurrentBook(const long long bookId)
	{
		m_idBook = bookId;
		m_setCurrentBookTimer->start();
	}

private:
	void Apply()
	{
		const auto& item = m_stack[m_current];
		Perform(&IObserver::OnApply, item.navigationMode, std::cref(item.navigationId), item.bookId);
		Perform(&IObserver::OnStateChanged);
	}

	void SetCurrentBookImpl()
	{
		if (Util::InBounds(m_current, 0, std::ssize(m_stack)))
		{
			const auto& item = m_stack[m_current];
			if (item.navigationMode == m_navigationMode && item.navigationId == m_navigationId && item.bookId == m_idBook)
				return;
		}

		m_stack.resize(++m_current);
		m_stack.emplace_back(m_navigationMode, m_navigationId, m_idBook);
		Perform(&IObserver::OnStateChanged);
	}

private:
	NavigationMode          m_navigationMode { NavigationMode::Unknown };
	QString                 m_navigationId;
	long long               m_idBook { -1 };
	std::unique_ptr<QTimer> m_setCurrentBookTimer { Util::CreateUiTimer([this] {
		SetCurrentBookImpl();
	}) };

	Stack     m_stack;
	ptrdiff_t m_current { -1 };
};

NavigationUndoRedo::NavigationUndoRedo()  = default;
NavigationUndoRedo::~NavigationUndoRedo() = default;

void NavigationUndoRedo::Undo()
{
	m_impl->Undo();
}

void NavigationUndoRedo::Redo()
{
	m_impl->Redo();
}

bool NavigationUndoRedo::IsUndoAvailable() const noexcept
{
	return m_impl->IsUndoAvailable();
}

bool NavigationUndoRedo::IsRedoAvailable() const noexcept
{
	return m_impl->IsRedoAvailable();
}

void NavigationUndoRedo::SetCurrentNavigation(const NavigationMode navigationMode, QString navigationId)
{
	m_impl->SetCurrentNavigation(navigationMode, std::move(navigationId));
}

void NavigationUndoRedo::SetCurrentBook(const long long bookId)
{
	m_impl->SetCurrentBook(bookId);
}

void NavigationUndoRedo::RegisterObserver(IObserver* observer)
{
	m_impl->Register(observer);
}

void NavigationUndoRedo::UnregisterObserver(IObserver* observer)
{
	m_impl->Unregister(observer);
}
