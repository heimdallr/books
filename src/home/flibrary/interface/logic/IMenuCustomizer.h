#pragma once

#include <expected>

#include "fnd/observer.h"

#include "IDataItem.h"

class QAction;
class QComboBox;
class QMenuBar;
class QWidget;

namespace HomeCompa::Flibrary
{

class IMenuCustomizer // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	enum class ItemAbility
	{
		None   = 0,
		Icon   = 1 << 0,
		Hotkey = 1 << 1,
		Hide   = 1 << 2,
		All    = Icon | Hotkey | Hide
	};

	class IObserver : public Observer
	{
	public:
		using RequestMenuCallback = std::function<void(const QString& id, const IDataItem::Ptr& item)>;

	public:
		virtual const char* GetRootKey() const noexcept                    = 0;
		virtual QWidget*    GetParentWidget() noexcept                     = 0;
		virtual void        RequestMenuItems(RequestMenuCallback callback) = 0;
		virtual void        OnHotkeyActivated(const QString& key)          = 0;
	};

public:
	virtual ~IMenuCustomizer() = default;

public:
	[[nodiscard]] virtual IDataItem::Ptr GetRootDataItem() const noexcept       = 0;
	[[nodiscard]] virtual ItemAbility    GetAbilities(const QString& key) const = 0;
	[[nodiscard]] virtual QString        GetHotkey(const QString& key) const    = 0;
	[[nodiscard]] virtual QVariant       GetIcon(const QString& key) const      = 0;
	[[nodiscard]] virtual QVariant       IsHidden(const QString& key) const     = 0;

	virtual void UpdateItems() = 0;

	virtual void    Add(const QString& rootKey, QWidget& widget, const QString& title)     = 0;
	virtual void    Add(const QString& rootKey, QMenuBar& menuBar, const QString& title)   = 0;
	virtual void    Add(const QString& rootKey, QComboBox& comboBox, const QString& title) = 0;
	virtual QString SetHotkey(const QString& key, const QString& shortCut = {})            = 0;
	virtual void    Hide(const QString& key, bool hidden)                                  = 0;

	virtual std::expected<void, QString> SetIcon(const QString& key, const QString& path = {}) = 0;

	virtual void RegisterObserver(IObserver* observer)   = 0;
	virtual void UnregisterObserver(IObserver* observer) = 0;
};

} // namespace HomeCompa::Flibrary

ENABLE_BITMASK_OPERATORS(HomeCompa::Flibrary::IMenuCustomizer::ItemAbility);
