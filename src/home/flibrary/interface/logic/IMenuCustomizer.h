#pragma once

#include <expected>

#include <QVariant>

#include "fnd/observer.h"

#include "IDataItem.h"

#include "export/flint.h"

class QAction;
class QComboBox;
class QMenuBar;
class QWidget;

namespace HomeCompa
{

class ISettings;

}

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

	struct Result
	{
		enum
		{
			Ok = 1,
			NeedReboot,
		};
	};

	class IItem // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		using Ptr = std::unique_ptr<IItem>;

	public:
		virtual ~IItem() = default;

		[[nodiscard]] virtual IDataItem::Ptr  GetItem() const noexcept          = 0;
		[[nodiscard]] virtual ItemAbility     GetAbilities() const noexcept     = 0;
		[[nodiscard]] virtual const QVariant& IsAddedToToolbar() const noexcept = 0;
		[[nodiscard]] virtual const QVariant& Hidden() const noexcept           = 0;

		virtual void SetHotkey(ISettings& settings, const QString& hotkey = {})                             = 0;
		virtual void SetIcon(ISettings& settings, const QVariant& value = {}, const QByteArray& bytes = {}) = 0;
		virtual void Hide(ISettings& settings, bool value)                                                  = 0;
		virtual void AddToToolbar(ISettings& settings, bool add)                                            = 0;
		virtual void SetAbilities(ItemAbility abilities) noexcept                                           = 0;
		virtual void SetEnabled(bool)                                                                       = 0;

		virtual void Setup(const ISettings& settings) = 0;

		[[nodiscard]] virtual QString  GetHotkey() const        = 0;
		[[nodiscard]] virtual QVariant GetIcon() const          = 0;
		[[nodiscard]] virtual QAction* GetToolbarAction() const = 0;
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

	class IToolbarController // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		virtual ~IToolbarController() = default;

		virtual void AddAction(const QString& key, QAction* action) = 0;
		virtual void RemoveAction(const QString& key)               = 0;
	};

public:
	virtual ~IMenuCustomizer() = default;

public:
	[[nodiscard]] virtual IDataItem::Ptr GetRootDataItem() const noexcept         = 0;
	[[nodiscard]] virtual ItemAbility    GetAbilities(const QString& key) const   = 0;
	[[nodiscard]] virtual QString        GetHotkey(const QString& key) const      = 0;
	[[nodiscard]] virtual QVariant       GetIcon(const QString& key) const        = 0;
	[[nodiscard]] virtual QVariant       IsHidden(const QString& key) const       = 0;
	[[nodiscard]] virtual QVariant       AddedToToolbar(const QString& key) const = 0;

	virtual void UpdateItems() = 0;

	virtual void    Add(const QString& rootKey, QWidget& widget, const QString& title)     = 0;
	virtual void    Add(const QString& rootKey, QMenuBar& menuBar, const QString& title)   = 0;
	virtual void    Add(const QString& rootKey, QComboBox& comboBox, const QString& title) = 0;
	virtual QString SetHotkey(const QString& key, const QString& shortCut = {})            = 0;
	virtual void    Hide(const QString& key, bool hidden)                                  = 0;
	virtual void    AddToToolbar(const QString& key, bool add)                             = 0;

	virtual std::expected<void, QString> SetIcon(const QString& key, const QString& path = {}) = 0;

	virtual void SetToolbarController(IToolbarController* toolbarController) noexcept = 0;

	virtual void RegisterObserver(IObserver* observer)   = 0;
	virtual void UnregisterObserver(IObserver* observer) = 0;

public:
	FLINT_EXPORT static bool IsHiddenByDefault(const QString& key);
};

} // namespace HomeCompa::Flibrary

ENABLE_BITMASK_OPERATORS(HomeCompa::Flibrary::IMenuCustomizer::ItemAbility);
