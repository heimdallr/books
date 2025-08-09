#pragma once

#include "fnd/observer.h"

namespace HomeCompa::Flibrary
{

class IAlphabetPanel // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	using ToolBars = std::vector<QToolBar*>;

public:
	static constexpr auto ID = "id";
	static constexpr auto TITLE = "title";
	static constexpr auto VISIBLE = "is_visible";

public:
	class IObserver : public Observer
	{
	public:
		virtual void OnToolBarChanged() = 0;
	};

public:
	virtual ~IAlphabetPanel() = default;
	virtual QWidget* GetWidget() noexcept = 0;
	virtual const ToolBars& GetToolBars() const = 0;

	virtual void RegisterObserver(IObserver* observer) = 0;
	virtual void UnregisterObserver(IObserver* observer) = 0;
};

} // namespace HomeCompa::Flibrary
