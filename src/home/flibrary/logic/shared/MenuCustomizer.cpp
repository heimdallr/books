#include "MenuCustomizer.h"

#include <ranges>

#include <QAction>
#include <QEvent>
#include <QEventLoop>
#include <QShortcut>

#include "fnd/IsOneOf.h"
#include "fnd/observable.h"

#include "interface/constants/SettingsConstant.h"
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
constexpr auto HOTKEY           = "hotkey";
constexpr auto HIDDEN           = "hidden";

TR_DEF

QString GetName(const QString& parent, const QString& child, const QString& key = {})
{
	return QString("%1%2%3").arg(parent.isEmpty() ? QString {} : QString("%1/").arg(parent), child, key.isEmpty() ? QString {} : QString("/%1").arg(key));
}

QString RemoveAmp(QString str)
{
	return str.remove('&');
}

template <typename T>
QString SetShortCutImpl(T& obj, const QKeySequence& value) = delete;

template <>
QString SetShortCutImpl<QAction>(QAction& obj, const QKeySequence& value)
{
	obj.setShortcut(value);
	return obj.shortcut().toString(QKeySequence::PortableText);
}

template <>
QString SetShortCutImpl<QShortcut>(QShortcut& obj, const QKeySequence& value)
{
	obj.setKey(value);
	return obj.key().toString(QKeySequence::PortableText);
}

bool IsHiddenByDefault(const QString& key)
{
	return IsOneOf(
		key,
		QString(Constant::Settings::NAVIGATION_HIDDEN_KEY_TEMPLATE).arg(NAVIGATION_NAMES[static_cast<size_t>(NavigationMode::AllBooks)].first),
		QString(Constant::Settings::NAVIGATION_HIDDEN_KEY_TEMPLATE).arg(NAVIGATION_NAMES[static_cast<size_t>(NavigationMode::AlreadyRead)].first),
		"Book context menu/Hash",
		"MainWindow/menuBar/menuSettings/actionAllSettings"
	);
}

} // namespace

class MenuCustomizer::Impl final
	: public QObject
	, public Observable<IObserver>
{
	struct Item
	{
		IDataItem::Ptr              item;
		propagate_const<QAction*>   action { nullptr };
		propagate_const<QShortcut*> shortcut { nullptr };
		propagate_const<IObserver*> observer { nullptr };
		ItemAbility                 abilities { ItemAbility::All };
		QVariant                    iconVar;
		QVariant                    hidden { false };

		QString GetHotkey() const
		{
			if (shortcut)
				return shortcut->key().toString();

			if (action)
				return action->shortcut().toString();

			return {};
		}

		QVariant GetIcon() const
		{
			if (!(abilities & ItemAbility::Icon))
				return {};

			if (action)
				if (auto icon = action->icon(); !icon.isNull())
					return icon;

			if (shortcut)
				if (const auto icon = shortcut->property(Constant::Settings::ICON); icon.isValid())
					return icon.value<QIcon>();

			if (iconVar.isValid() && iconVar.canConvert<QIcon>())
				return iconVar.value<QIcon>();

			return {};
		}

		QString SetShortCut(const QString& value = {})
		{
			item->SetData(value, SettingsItem::Column::Value);

			const auto keySequence = value.isEmpty() ? QKeySequence {} : QKeySequence(value, QKeySequence::PortableText);

			if (action)
				return SetShortCutImpl(*action, keySequence);

			if (!shortcut)
			{
				if (value.isEmpty())
					return value;

				assert(observer);
				shortcut = new QShortcut(keySequence, observer->GetParentWidget());
				connect(shortcut.get(), &QShortcut::activated, [o = observer.get(), key = item->GetData(SettingsItem::Column::Key)] {
					o->OnHotkeyActivated(key);
				});
			}

			return SetShortCutImpl(*shortcut, keySequence);
		}

		void SetIcon(const QVariant& value = {})
		{
			if (action)
				return action->setIcon(value.value<QIcon>());

			if (shortcut)
				return (void)shortcut->setProperty(Constant::Settings::ICON, value);

			iconVar = value;
		}

		void Hide(const bool value)
		{
			hidden = value;
			if (action)
				action->setVisible(!value);
		}

		void Setup(const ISettings& settings)
		{
			if (const auto var = settings.Get(HOTKEY); var.isValid())
				SetShortCut(var.toString());

			if (const auto var = settings.Get(HIDDEN); var.isValid())
				Hide(var.toBool());
			else if (IsHiddenByDefault(item->GetData(SettingsItem::Column::Key)))
				Hide(true);

			if (const auto var = settings.Get(Constant::Settings::ICON); var.isValid())
			{
				const auto bytes = var.toByteArray();
				if (bytes.isEmpty())
					return SetIcon();

				if (const auto pixmap = Util::Decode(bytes); !pixmap.isNull())
					return SetIcon(QVariant::fromValue(QIcon(pixmap)));

				SetIcon();
			}
		}
	};

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
			return it->second.abilities;
		return ItemAbility::All;
	}

	QString GetHotkey(const QString& key) const
	{
		if (const auto it = m_actions.find(key); it != m_actions.end())
			return it->second.GetHotkey();
		return {};
	}

	QVariant GetIcon(const QString& key) const
	{
		if (const auto it = m_actions.find(key); it != m_actions.end())
			return it->second.GetIcon();
		return {};
	}

	QVariant IsHidden(const QString& key) const
	{
		if (const auto it = m_actions.find(key); it != m_actions.end())
			return it->second.hidden;

		if (!m_actions.empty())
			return {};

		if (const auto var = m_settings->Get(GetName(MENU_CUSTOM_ROOT, key, HIDDEN)); var.isValid())
			return var.toBool();

		if (IsHiddenByDefault(key))
			return true;

		return {};
	}

	void UpdateItems()
	{
		UpdateObserverMenu();
	}

	template <typename R>
	using UpdateItemFunctor = std::function<void(Item&, R*)>;

	template <typename W, typename R>
	using UiFactoryToHotkeys = std::pair<IDataItem::Ptr, QObject*> (IUiFactory::*)(const QString&, W&, const QString&, const IUiFactory::MenuCustomizeFunctor<R>&) const;

	template <typename W, typename R>
	void Add(const QString& rootKey, W& widget, const QString& title, const UiFactoryToHotkeys<W, R> uiFactoryToHotkeys, const UpdateItemFunctor<R>& f)
	{
		const auto toMenuCustomizeFunctor = [&](IDataItem::Ptr actionItem, const ItemAbility abilities, QObject* parent, R* r) {
			Item item { .item = std::move(actionItem), .abilities = abilities };
			f(item, r);
			Add(std::move(item), parent);
		};

		auto [item, obj] = std::invoke(uiFactoryToHotkeys, std::cref(*m_uiFactory), std::cref(rootKey), std::ref(widget), std::cref(title), std::cref(toMenuCustomizeFunctor));
		if (const auto it = m_objToActions.find(obj); it != m_objToActions.end())
			it->second.key = item->GetData(SettingsItem::Column::Key);

		m_root->AppendChild(std::move(item));
	}

	void Add(const QString& rootKey, QWidget& widget, const QString& title)
	{
		Add<QWidget, QAction>(rootKey, widget, title, &IUiFactory::AddWidgetToMenuCustomizer, [](Item& item, QAction* action) {
			item.action = action;
		});
	}

	void Add(const QString& rootKey, QMenuBar& menuBar, const QString& title)
	{
		Add<QMenuBar, QAction>(rootKey, menuBar, title, &IUiFactory::AddMenuBarToMenuCustomizer, [](Item& item, QAction* action) {
			item.action = action;
		});
	}

	void Add(const QString& rootKey, QComboBox& comboBox, const QString& title)
	{
		Add<QComboBox, QShortcut>(rootKey, comboBox, title, &IUiFactory::AddComboBoxToMenuCustomizer, [](Item& item, QShortcut* shortcut) {
			item.shortcut = shortcut;
		});
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

		const auto value = it->second.SetShortCut(shortCut);
		m_settings->Set(GetName(MENU_CUSTOM_ROOT, it->second.item->GetData(SettingsItem::Column::Key), HOTKEY), value);
		return {};
	}

	void ResetHotkey(const QString& key)
	{
		const auto it = m_actions.find(key);
		if (it == m_actions.end())
			return;

		it->second.SetShortCut();
		m_settings->Remove(GetName(MENU_CUSTOM_ROOT, it->second.item->GetData(SettingsItem::Column::Key), HOTKEY));
	}

	void Hide(const QString& key, const bool hidden)
	{
		const auto it = m_actions.find(key);
		if (it == m_actions.end())
			return;

		it->second.Hide(hidden);
		m_settings->Set(GetName(MENU_CUSTOM_ROOT, it->second.item->GetData(SettingsItem::Column::Key), HIDDEN), hidden);
	}

	std::expected<void, QString> SetIcon(const QString& key, const QString& path)
	{
		const auto it = m_actions.find(key);
		if (it == m_actions.end())
			return {};

		if (path.isEmpty())
		{
			m_settings->Set(GetName(MENU_CUSTOM_ROOT, it->second.item->GetData(SettingsItem::Column::Key), Constant::Settings::ICON), QString {});
			it->second.SetIcon();
			return {};
		}

		QFile file(path);
		if (!file.open(QIODevice::ReadOnly))
			return std::unexpected(Tr(CANNOT_OPEN).arg(path));

		const auto bytes = file.readAll();
		if (bytes.isEmpty())
			return std::unexpected(Tr(FILE_EMPTY).arg(path));

		if (const auto pixmap = Util::Decode(bytes); !pixmap.isNull())
		{
			m_settings->Set(GetName(MENU_CUSTOM_ROOT, it->second.item->GetData(SettingsItem::Column::Key), Constant::Settings::ICON), bytes);
			it->second.SetIcon(QVariant::fromValue(QIcon(pixmap)));
			return {};
		}

		return std::unexpected(Tr(BAD_IMAGE).arg(path));
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
				it = m_actions
				         .try_emplace(
							 itemKey,
							 Item {
								 .item      = std::move(child),
								 .observer  = observer,
								 .abilities = parentKey.isEmpty() ? ItemAbility::None : ItemAbility::All,
							 }
						 )
				         .first;
			}

			it->second.Setup(*m_settings);

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
				if (itAction->second.shortcut)
					itAction->second.shortcut->setEnabled(enabled);
	}

	void Add(Item item, QObject* parent)
	{
		auto key = item.item->GetData(SettingsItem::Column::Key);
		{
			const SettingsGroup settingsSubGroup(*m_settings, GetName(MENU_CUSTOM_ROOT, key));
			item.Setup(*m_settings);
		}
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

				auto& dstChildRef = [&]() ->IDataItem& {
					if (const auto it = m_actions.find(key); it != m_actions.end())
					{
						it->second.item->SetData(RemoveAmp(std::move(title)), SettingsItem::Column::Title);
						if (id == -1)
							it->second.abilities = ~ItemAbility::Hotkey;
						return *dst.AppendChild(it->second.item);
					}

					auto dstChild = dst.AppendChild(SettingsItem::Create());
					dstChild->SetData(std::move(key), SettingsItem::Column::Key);
					dstChild->SetData(RemoveAmp(std::move(title)), SettingsItem::Column::Title);

					auto& ref = *dstChild;
					Item  childItem { .item = std::move(dstChild), .observer = observer, .abilities = id == -1 ? ~ItemAbility::Hotkey : ItemAbility::All };
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

	std::map<QString, Item> m_actions;

	std::unordered_map<const QObject*, ObjToActions> m_objToActions;
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

std::expected<void, QString> MenuCustomizer::SetIcon(const QString& key, const QString& path)
{
	return m_impl->SetIcon(key, path);
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
