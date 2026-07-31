#pragma once

#include <expected>

#include "fnd/observer.h"

#include "IDataItem.h"

class QAction;
class QComboBox;
class QMenuBar;
class QWidget;
class QIcon;

namespace HomeCompa::Flibrary
{

class IHotkeyManager // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IObserver : public Observer
	{
	public:
		using RequestMenuCallback = std::function<void(const QString& id, const IDataItem::Ptr& item)>;

	public:
		virtual const char* GetKey() const noexcept                        = 0;
		virtual QWidget*    GetParentWidget() noexcept                     = 0;
		virtual void        RequestMenuItems(RequestMenuCallback callback) = 0;
		virtual void        OnHotkeyActivated(const QString& key)          = 0;
	};

public:
	virtual ~IHotkeyManager() = default;

public:
	[[nodiscard]] virtual IDataItem::Ptr GetRootDataItem()                            = 0;
	[[nodiscard]] virtual bool           HasHotkey(const QString& key) const noexcept = 0;
	[[nodiscard]] virtual QIcon          GetIcon(const QString& key) const            = 0;

	virtual void    Add(QWidget& widget, const QString& title)       = 0;
	virtual void    Add(QMenuBar& menuBar, const QString& title)     = 0;
	virtual void    Add(QComboBox& comboBox, const QString& title)   = 0;
	virtual QString Set(const QString& key, const QString& shortCut) = 0;
	virtual void    Reset(const QString& key)                        = 0;

	virtual std::expected<void, QString> SetIcon(const QString& key, const QString& path = {}) = 0;

	virtual void RegisterObserver(IObserver* observer)   = 0;
	virtual void UnregisterObserver(IObserver* observer) = 0;
};

} // namespace HomeCompa::Flibrary
