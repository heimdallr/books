#include "MenuCustomizer.h"

#include <ranges>

#include <QAction>
#include <QEvent>
#include <QEventLoop>
#include <QShortcut>

#include "fnd/observable.h"

#include "interface/localization.h"

#include "data/DataItem.h"
#include "util/ImageUtil.h"

#include "BookInteractor.h"
#include "log.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace
{

constexpr auto CONTEXT     = "HotkeyManager";
constexpr auto CANNOT_OPEN = QT_TRANSLATE_NOOP("HotkeyManager", "Cannot open '%1'");
constexpr auto FILE_EMPTY  = QT_TRANSLATE_NOOP("HotkeyManager", "File %1 is empty");
constexpr auto BAD_IMAGE   = QT_TRANSLATE_NOOP("HotkeyManager", "Image %1 probably corrupted");

constexpr auto MENU_CUSTOM_ROOT = "ui/MenuCustomization";

TR_DEF

QString GetName(const QString& parent, const QString& child, const QString& key = {})
{
	return QString("%1%2%3").arg(parent.isEmpty() ? QString {} : QString("%1/").arg(parent), child, key.isEmpty() ? QString {} : QString("/%1").arg(key));
}

QString RemoveAmp(QString str)
{
	return str.remove('&');
}

} // namespace

class MenuCustomizer::Impl final
	: public QObject
	, public Observable<IObserver>
{
	struct ObjToActions
	{
		QString              key;
		bool                 enabled { true };
		std::vector<QString> actionKeys;
	};

public:
	explicit Impl(std::shared_ptr<const IParentWidgetProvider> parentWidgetProvider, std::shared_ptr<const IUiFactory> uiFactory, std::shared_ptr<ISettings> settings)
		: m_parentWidgetProvider { std::move(parentWidgetProvider) }
		, m_uiFactory { std::move(uiFactory) }
		, m_settings { std::move(settings) }
	{
	}

	IDataItem::Ptr GetRootDataItem() const noexcept
	{
		return m_root;
	}

	ItemAbility GetAbilities(const QString& key) const
	{
		if (const auto it = m_actions.find(key); it != m_actions.end())
			return it->second->GetAbilities();
		return ItemAbility::All;
	}

	QString GetHotkey(const QString& key) const
	{
		if (const auto it = m_actions.find(key); it != m_actions.end())
			return it->second->GetHotkey();
		return {};
	}

	QVariant GetIcon(const QString& key) const
	{
		if (const auto it = m_actions.find(key); it != m_actions.end())
			return it->second->GetIcon();
		return {};
	}

	QVariant IsHidden(const QString& key) const
	{
		if (const auto it = m_actions.find(key); it != m_actions.end())
			return it->second->Hidden();

		if (!m_actions.empty())
			return {};

		if (IsHiddenByDefault(key))
			return true;

		return {};
	}

	QVariant AddedToToolbar(const QString& key) const
	{
		if (const auto it = m_actions.find(key); it != m_actions.end())
			return it->second->IsAddedToToolbar();

		return {};
	}

	void UpdateItems()
	{
		UpdateObserverMenu();
	}

	template <typename W>
	using UiFactoryToHotkeys = std::pair<IDataItem::Ptr, QObject*> (IUiFactory::*)(const QString&, W&, const QString&, const IUiFactory::MenuCustomizeFunctor&) const;

	template <typename W>
	void Add(const QString& rootKey, W& widget, const QString& title, const UiFactoryToHotkeys<W> uiFactoryToHotkeys)
	{
		const auto toMenuCustomizeFunctor = [&](IItem::Ptr item, QObject* parent) {
			Add(std::move(item), parent);
		};

		auto [item, obj] = std::invoke(uiFactoryToHotkeys, std::cref(*m_uiFactory), std::cref(rootKey), std::ref(widget), std::cref(title), std::cref(toMenuCustomizeFunctor));
		if (const auto it = m_objToActions.find(obj); it != m_objToActions.end())
			it->second.key = item->GetData(SettingsItem::Column::Key);

		m_root->AppendChild(std::move(item));
	}

	void Add(const QString& rootKey, QWidget& widget, const QString& title)
	{
		Add<QWidget>(rootKey, widget, title, &IUiFactory::AddWidgetToMenuCustomizer);
	}

	void Add(const QString& rootKey, QMenuBar& menuBar, const QString& title)
	{
		Add<QMenuBar>(rootKey, menuBar, title, &IUiFactory::AddMenuBarToMenuCustomizer);
	}

	void Add(const QString& rootKey, QComboBox& comboBox, const QString& title)
	{
		Add<QComboBox>(rootKey, comboBox, title, &IUiFactory::AddComboBoxToMenuCustomizer);
	}

	QString SetHotkey(const QString& key, const QString& shortCut)
	{
		const auto it = m_actions.find(key);
		assert(it != m_actions.end());

		const auto disabled = m_objToActions | std::views::filter([](const auto& item) {
								  return !item.second.enabled;
							  })
		                    | std::views::transform([](const auto& item) {
								  return item.second.key;
							  })
		                    | std::ranges::to<std::unordered_set>();

		const auto findCollision = [&](const IDataItem& parent, const auto& r) -> IDataItem::Ptr {
			for (size_t i = 0, sz = parent.GetChildCount(); i < sz; ++i)
			{
				auto       child    = parent.GetChild(i);
				const auto childKey = child->GetData(SettingsItem::Column::Key);
				if (disabled.contains(childKey))
					continue;

				if (child->GetData(SettingsItem::Column::Value) == shortCut && childKey != key)
					return child;

				if (auto result = r(*child, r))
					return result;
			}
			return {};
		};

		if (const auto found = findCollision(*m_root, findCollision))
		{
			QStringList names;
			auto*       item = found.get();
			while (item)
			{
				if (auto title = item->GetData(SettingsItem::Column::Title); !title.isEmpty())
					names.push_front(std::move(title));
				item = item->GetParent();
			}
			return names.join(" / ");
		}

		it->second->SetHotkey(*m_settings, shortCut);
		return {};
	}

	void ResetHotkey(const QString& key)
	{
		if (const auto it = m_actions.find(key); it != m_actions.end())
			it->second->SetHotkey(*m_settings);
	}

	void Hide(const QString& key, const bool hidden)
	{
		if (const auto it = m_actions.find(key); it != m_actions.end())
			it->second->Hide(*m_settings, hidden);
	}

	void AddToToolbar(const QString& key, const bool add)
	{
		const auto it = m_actions.find(key);
		if (it == m_actions.end())
			return;

		it->second->AddToToolbar(*m_settings, add);

		if (m_toolbarController)
			add ? m_toolbarController->AddAction(key, it->second->GetToolbarAction()) : m_toolbarController->RemoveAction(key);
	}

	std::expected<void, QString> SetIcon(const QString& key, const QString& path)
	{
		const auto it = m_actions.find(key);
		if (it == m_actions.end())
			return {};

		assert(it->second->GetItem()->GetData(SettingsItem::Column::Key) == key);
		if (path.isEmpty())
			return it->second->SetIcon(*m_settings), std::expected<void, QString> {};

		QFile file(path);
		if (!file.open(QIODevice::ReadOnly))
			return std::unexpected(Tr(CANNOT_OPEN).arg(path));

		const auto bytes = file.readAll();
		if (bytes.isEmpty())
			return std::unexpected(Tr(FILE_EMPTY).arg(path));

		if (const auto pixmap = Util::Decode(bytes); !pixmap.isNull())
			return it->second->SetIcon(*m_settings, QVariant::fromValue(QIcon(pixmap)), bytes), std::expected<void, QString> {};

		return std::unexpected(Tr(BAD_IMAGE).arg(path));
	}

	void SetToolbarController(IToolbarController* toolbarController) noexcept
	{
		m_toolbarController = toolbarController;
		for (auto&& [key, actionItem] : m_actions)
			if (auto* action = actionItem->GetToolbarAction())
				m_toolbarController->AddAction(key, action);
	}

	void AddObserver(IObserver* observer)
	{
		auto& objToActions = Add(observer->GetParentWidget());
		objToActions.key   = observer->GetRootKey();

		std::unordered_set<QString> uniqueKeys;

		const auto enumerate = [&](const QString& parentKey, const QString& key, const auto& r) -> void {
			const auto itemKey = GetName(parentKey, key);
			auto       it      = m_actions.find(itemKey);
			if (it == m_actions.end())
			{
				auto child = SettingsItem::Create();
				child->SetData(itemKey, SettingsItem::Column::Key);
				it = m_actions.try_emplace(itemKey, m_uiFactory->CreateMenuCustomizerItem(std::move(child), *observer)).first;
				it->second->SetAbilities(parentKey.isEmpty() ? ItemAbility::None : ItemAbility::All);
			}

			it->second->Setup(*m_settings);

			if (m_toolbarController)
				if (auto* action = it->second->GetToolbarAction())
					m_toolbarController->AddAction(itemKey, action);

			for (const auto& group : m_settings->GetGroups())
			{
				const SettingsGroup settingsSubGroup(*m_settings, group);
				r(itemKey, group, r); // NOLINT(readability-suspicious-call-argument)
			}
		};
		const SettingsGroup settingsSubGroup(*m_settings, GetName(MENU_CUSTOM_ROOT, observer->GetRootKey()));
		enumerate({}, observer->GetRootKey(), enumerate);
	}

	void RemoveObserver(IObserver* observer)
	{
		Remove(observer->GetParentWidget());
	}

private: // QObject
	bool eventFilter(QObject* watched, QEvent* event) override
	{
		switch (event->type()) // NOLINT(clang-diagnostic-switch-enum)
		{
			case QEvent::Hide:
				EnabledChanged(watched, false);
				break;

			case QEvent::Show:
				EnabledChanged(watched, true);
				break;

			default:
				break;
		}
		return QObject::eventFilter(watched, event);
	}

private:
	void EnabledChanged(const QObject* parent, const bool enabled)
	{
		const auto it = m_objToActions.find(parent);
		if (it == m_objToActions.end())
			return;

		it->second.enabled = enabled;

		for (const auto& key : it->second.actionKeys)
			if (const auto itAction = m_actions.find(key); itAction != m_actions.end())
				itAction->second->SetEnabled(enabled);
	}

	void Add(IItem::Ptr item, QObject* parent)
	{
		auto key = item->GetItem()->GetData(SettingsItem::Column::Key);
		{
			const SettingsGroup settingsSubGroup(*m_settings, GetName(MENU_CUSTOM_ROOT, key));
			item->Setup(*m_settings);
		}
		if (m_toolbarController)
			if (auto* action = item->GetToolbarAction())
				m_toolbarController->AddAction(key, action);

		const auto [it, added] = m_actions.try_emplace(std::move(key), std::move(item));
		assert(added);

		auto& actionKeys = Add(parent).actionKeys;
		actionKeys.emplace_back(it->first);
	}

	ObjToActions& Add(QObject* parent)
	{
		auto [it, inserted] = m_objToActions.try_emplace(parent);
		if (inserted)
		{
			parent->installEventFilter(this);
			connect(parent, &QObject::destroyed, this, &Impl::Remove);
		}

		return it->second;
	}

	void Remove(QObject* parent)
	{
		const auto it = m_objToActions.find(parent);
		if (it == m_objToActions.end())
			return;

		parent->removeEventFilter(this);

		if (const auto menu = m_root->FindChild([&key = it->second.key](const auto& item) {
				return item.GetData(SettingsItem::Column::Key) == key;
			}))
			m_root->RemoveChild(menu->GetRow());

		for (const auto& key : it->second.actionKeys)
			m_actions.erase(key);
		m_objToActions.erase(it);
	}

	void UpdateObserverMenu()
	{
		Perform([this](const IObserver* observer) {
			if (const auto bookMenu = m_root->FindChild([key = observer->GetRootKey()](const auto& item) {
					return item.GetData(SettingsItem::Column::Key) == key;
				}))
				m_root->RemoveChild(bookMenu->GetRow());
		});

		auto count = GetObserverCount();

		QEventLoop eventLoop;

		Perform([&](IObserver* observer) {
			observer->RequestMenuItems([this, observer, &count, &eventLoop](const QString&, const IDataItem::Ptr& item) {
				UpdateObserverMenu(observer, item);
				if (--count == 0)
					eventLoop.exit();
			});
		});

		eventLoop.exec();
	}

	void UpdateObserverMenu(IObserver* observer, const IDataItem::Ptr& item)
	{
		if (item->GetChildCount() == 0)
			return;

		const auto enumerate = [this, observer](const IDataItem& src, IDataItem& dst, const auto& r) -> void {
			for (size_t i = 0, sz = src.GetChildCount(); i < sz; ++i)
			{
				const auto srcChild = src.GetChild(i);
				const auto id       = srcChild->GetData(MenuItem::Column::Id).toInt();
				auto       title    = srcChild->GetData(MenuItem::Column::Title);
				if (id == 0 || title.isEmpty())
					continue;

				auto key = GetName(dst.GetData(SettingsItem::Column::Key), srcChild->GetData(MenuItem::Column::Key));

				auto& dstChildRef = [&]() -> IDataItem& {
					if (const auto it = m_actions.find(key); it != m_actions.end())
					{
						it->second->GetItem()->SetData(RemoveAmp(std::move(title)), SettingsItem::Column::Title);
						if (id == -1)
							it->second->SetAbilities(it->second->GetAbilities() & ~ItemAbility::Hotkey);
						return *dst.AppendChild(it->second->GetItem());
					}

					auto dstChild = dst.AppendChild(SettingsItem::Create());
					dstChild->SetData(std::move(key), SettingsItem::Column::Key);
					dstChild->SetData(RemoveAmp(std::move(title)), SettingsItem::Column::Title);

					auto& ref       = *dstChild;
					auto  childItem = m_uiFactory->CreateMenuCustomizerItem(std::move(dstChild), *observer);
					childItem->SetAbilities(id == -1 ? ~ItemAbility::Hotkey : ItemAbility::All);
					Add(std::move(childItem), observer->GetParentWidget());
					return ref;
				}();

				r(*srcChild, dstChildRef, r);
			}
		};

		auto& rootObserverItem = m_root->AppendChild(SettingsItem::Create());
		rootObserverItem->SetData(observer->GetRootKey(), SettingsItem::Column::Key);
		rootObserverItem->SetData(Tr(observer->GetRootKey()), SettingsItem::Column::Title);
		enumerate(*item, *rootObserverItem, enumerate);
	}

private:
	std::shared_ptr<const IParentWidgetProvider>  m_parentWidgetProvider;
	std::shared_ptr<const IUiFactory>             m_uiFactory;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;

	IDataItem::Ptr m_root { SettingsItem::Create() };

	std::map<QString, IItem::Ptr> m_actions;

	std::unordered_map<const QObject*, ObjToActions> m_objToActions;

	propagate_const<IToolbarController*> m_toolbarController;
};

MenuCustomizer::MenuCustomizer(std::shared_ptr<const IParentWidgetProvider> parentWidgetProvider, std::shared_ptr<const IUiFactory> uiFactory, std::shared_ptr<ISettings> settings)
	: m_impl(std::move(parentWidgetProvider), std::move(uiFactory), std::move(settings))
{
}

MenuCustomizer::~MenuCustomizer() = default;

IDataItem::Ptr MenuCustomizer::GetRootDataItem() const noexcept
{
	return m_impl->GetRootDataItem();
}

IMenuCustomizer::ItemAbility MenuCustomizer::GetAbilities(const QString& key) const
{
	return m_impl->GetAbilities(key);
}

QString MenuCustomizer::GetHotkey(const QString& key) const
{
	return m_impl->GetHotkey(key);
}

QVariant MenuCustomizer::GetIcon(const QString& key) const
{
	return m_impl->GetIcon(key);
}

QVariant MenuCustomizer::IsHidden(const QString& key) const
{
	return m_impl->IsHidden(key);
}

QVariant MenuCustomizer::AddedToToolbar(const QString& key) const
{
	return m_impl->AddedToToolbar(key);
}

void MenuCustomizer::UpdateItems()
{
	m_impl->UpdateItems();
}

void MenuCustomizer::Add(const QString& rootKey, QWidget& widget, const QString& title)
{
	m_impl->Add(rootKey, widget, title);
}

void MenuCustomizer::Add(const QString& rootKey, QMenuBar& menuBar, const QString& title)
{
	m_impl->Add(rootKey, menuBar, title);
}

void MenuCustomizer::Add(const QString& rootKey, QComboBox& comboBox, const QString& title)
{
	m_impl->Add(rootKey, comboBox, title);
}

QString MenuCustomizer::SetHotkey(const QString& key, const QString& shortCut)
{
	return shortCut.isEmpty() ? (m_impl->ResetHotkey(key), QString {}) : m_impl->SetHotkey(key, shortCut);
}

void MenuCustomizer::Hide(const QString& key, const bool hidden)
{
	m_impl->Hide(key, hidden);
}

void MenuCustomizer::AddToToolbar(const QString& key, const bool add)
{
	m_impl->AddToToolbar(key, add);
}

std::expected<void, QString> MenuCustomizer::SetIcon(const QString& key, const QString& path)
{
	return m_impl->SetIcon(key, path);
}

void MenuCustomizer::SetToolbarController(IToolbarController* toolbarController) noexcept
{
	m_impl->SetToolbarController(toolbarController);
}

void MenuCustomizer::RegisterObserver(IObserver* observer)
{
	m_impl->AddObserver(observer);
	m_impl->Register(observer);
}

void MenuCustomizer::UnregisterObserver(IObserver* observer)
{
	m_impl->RemoveObserver(observer);
	m_impl->Unregister(observer);
}
