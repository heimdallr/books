#include "HotkeyManager.h"

#include <ranges>

#include <QAction>
#include <QEvent>
#include <QEventLoop>
#include <QShortcut>

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

TR_DEF

QString GetName(const QString& parent, const QString& child)
{
	return QString("%1/%2").arg(parent, child);
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

} // namespace

class HotkeyManager::Impl final
	: public QObject
	, public Observable<IObserver>
{
	struct Item
	{
		IDataItem::Ptr              item;
		propagate_const<QAction*>   action { nullptr };
		propagate_const<QShortcut*> shortcut { nullptr };
		propagate_const<IObserver*> observer { nullptr };
		mutable QVariant            iconVar;

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

		bool HasHotkey() const
		{
			return !(!shortcut && (!action || action->shortcut().isEmpty()));
		}

		void SetIcon(const QVariant& value = {})
		{
			if (action)
				return action->setIcon(value.value<QIcon>());

			if (shortcut)
				return (void)shortcut->setProperty(Constant::Settings::ICON, value);

			iconVar = value;
		}

		QVariant GetIcon() const
		{
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

	IDataItem::Ptr GetRootDataItem()
	{
		UpdateObserverMenu();
		return m_root;
	}

	bool HasHotkey(const QString& key) const noexcept
	{
		const auto it = m_actions.find(key);
		return it != m_actions.end() && it->second.HasHotkey();
	}

	QVariant GetIcon(const QString& key) const
	{
		if (const auto it = m_actions.find(key); it != m_actions.end())
			return it->second.GetIcon();
		return {};
	}

	template <typename W, typename R>
	using UiFactoryToHotkeys = std::pair<IDataItem::Ptr, QObject*> (IUiFactory::*)(const QString&, W&, const QString&, const std::function<void(IDataItem::Ptr, R*, QObject*)>&) const;

	template <typename W, typename R>
	void Add(const QString& rootKey, W& widget, const QString& title, const UiFactoryToHotkeys<W, R> uiFactoryToHotkeys, const std::function<void(IDataItem::Ptr, R*, QObject*)>& f)
	{
		auto [item, obj] = std::invoke(uiFactoryToHotkeys, std::cref(*m_uiFactory), std::cref(rootKey), std::ref(widget), std::cref(title), std::cref(f));
		if (const auto it = m_objToActions.find(obj); it != m_objToActions.end())
			it->second.key = item->GetData(SettingsItem::Column::Key);
		m_root->AppendChild(std::move(item));
	}

	void Add(const QString& rootKey, QWidget& widget, const QString& title)
	{
		Add<QWidget, QAction>(rootKey, widget, title, &IUiFactory::AddWidgetToHotkeys, [&](IDataItem::Ptr actionItem, QAction* action, QObject* parent) {
			Add(Item { .item = std::move(actionItem), .action = action }, parent);
		});
	}

	void Add(const QString& rootKey, QMenuBar& menuBar, const QString& title)
	{
		Add<QMenuBar, QAction>(rootKey, menuBar, title, &IUiFactory::AddMenuBarToHotkeys, [&](IDataItem::Ptr actionItem, QAction* action, QObject* parent) {
			Add(Item { .item = std::move(actionItem), .action = action }, parent);
		});
	}

	void Add(const QString& rootKey, QComboBox& comboBox, const QString& title)
	{
		Add<QComboBox, QShortcut>(rootKey, comboBox, title, &IUiFactory::AddComboBoxToHotkeys, [&](IDataItem::Ptr actionItem, QShortcut* shortcut, QObject* parent) {
			Add(Item { .item = std::move(actionItem), .shortcut = shortcut }, parent);
		});
	}

	QString Set(const QString& key, const QString& shortCut)
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
		m_settings->Set(GetName(Constant::Settings::HOTKEYS_ROOT, it->second.item->GetData(SettingsItem::Column::Key)), value);
		return {};
	}

	void Reset(const QString& key)
	{
		const auto it = m_actions.find(key);
		assert(it != m_actions.end());

		it->second.SetShortCut();
		m_settings->Remove(GetName(Constant::Settings::HOTKEYS_ROOT, it->second.item->GetData(SettingsItem::Column::Key)));
	}

	std::expected<void, QString> SetIcon(const QString& key, const QString& path)
	{
		const auto it = m_actions.find(key);
		assert(it != m_actions.end());

		if (path.isEmpty())
		{
			m_settings->Set(GetName(Constant::Settings::ICONS_ROOT, it->second.item->GetData(SettingsItem::Column::Key)), QString {});
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
			m_settings->Set(GetName(Constant::Settings::ICONS_ROOT, it->second.item->GetData(SettingsItem::Column::Key)), bytes);
			it->second.SetIcon(QVariant::fromValue(QIcon(pixmap)));
			return {};
		}

		return std::unexpected(Tr(BAD_IMAGE).arg(path));
	}

	void AddObserver(IObserver* observer)
	{
		auto& objToActions = Add(observer->GetParentWidget());
		objToActions.key   = observer->GetHotkeyRootKey();

		std::unordered_set<QString> uniqueKeys;

		const auto enumerate = [&](const QString& key, const auto& r, const auto& f) -> void {
			for (const auto& k : m_settings->GetKeys())
			{
				if (const auto value = m_settings->Get(k); value.isValid())
				{
					auto itemKey = GetName(key, k);
					auto it      = m_actions.find(itemKey);
					if (it == m_actions.end())
					{
						auto child = SettingsItem::Create();
						child->SetData(itemKey, SettingsItem::Column::Key);
						it = m_actions
						         .try_emplace(
									 itemKey,
									 Item {
										 .item     = std::move(child),
										 .observer = observer,
									 }
								 )
						         .first;
					}

					if (uniqueKeys.emplace(itemKey).second)
						objToActions.actionKeys.emplace_back(std::move(itemKey));

					f(it->second, value);
				}
			}

			for (const auto& group : m_settings->GetGroups())
			{
				const SettingsGroup settingsSubGroup(*m_settings, group);
				r(GetName(key, group), r, f);
			}
		};

		{
			const SettingsGroup settingsGroup(*m_settings, GetName(Constant::Settings::HOTKEYS_ROOT, observer->GetHotkeyRootKey()));
			enumerate(observer->GetHotkeyRootKey(), enumerate, [](Item& item, const QVariant& value) {
				item.SetShortCut(value.toString());
			});
		}
		{
			const SettingsGroup settingsGroup(*m_settings, GetName(Constant::Settings::ICONS_ROOT, observer->GetHotkeyRootKey()));
			enumerate(observer->GetHotkeyRootKey(), enumerate, [](const Item& item, const QVariant& value) {
				const auto bytes = value.toByteArray();
				if (bytes.isEmpty())
					return item.iconVar.clear();

				if (const auto pixmap = Util::Decode(bytes); !pixmap.isNull())
					return (void)(item.iconVar = QIcon(pixmap));

				item.iconVar.clear();
			});
		}
	}

	void RemoveObserver(IObserver* observer)
	{
		Remove(observer->GetParentWidget());
	}

private: // QObject
	bool eventFilter(QObject* watched, QEvent* event) override
	{
		switch (event->type())
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
		auto  key              = item.item->GetData(SettingsItem::Column::Key);
		auto& objToActions     = Add(parent);
		const auto [it, added] = m_actions.try_emplace(std::move(key), std::move(item));
		assert(added);
		objToActions.actionKeys.emplace_back(it->first);
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
			if (const auto bookMenu = m_root->FindChild([key = observer->GetHotkeyRootKey()](const auto& item) {
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
				auto       title    = srcChild->GetData(MenuItem::Column::Title);
				if (title.isEmpty())
					continue;

				auto key = GetName(dst.GetData(SettingsItem::Column::Key), srcChild->GetData(MenuItem::Column::Key));
				if (const auto it = m_actions.find(key); it != m_actions.end())
				{
					it->second.item->SetData(RemoveAmp(std::move(title)), SettingsItem::Column::Title);
					dst.AppendChild(it->second.item);
					continue;
				}

				auto& dstChild = dst.AppendChild(SettingsItem::Create());
				dstChild->SetData(std::move(key), SettingsItem::Column::Key);
				dstChild->SetData(RemoveAmp(std::move(title)), SettingsItem::Column::Title);

				if (!srcChild->GetData(MenuItem::Column::Id).isEmpty())
					Add(Item { .item = dstChild, .observer = observer }, observer->GetParentWidget());

				r(*srcChild, *dstChild, r);
			}
		};

		const auto& bookItem = m_root->AppendChild(SettingsItem::Create());
		bookItem->SetData(observer->GetHotkeyRootKey(), SettingsItem::Column::Key);
		bookItem->SetData(Tr(observer->GetHotkeyRootKey()), SettingsItem::Column::Title);
		enumerate(*item, *bookItem, enumerate);
	}

private:
	std::shared_ptr<const IParentWidgetProvider>  m_parentWidgetProvider;
	std::shared_ptr<const IUiFactory>             m_uiFactory;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;

	IDataItem::Ptr m_root { SettingsItem::Create() };

	std::map<QString, Item> m_actions;

	std::unordered_map<const QObject*, ObjToActions> m_objToActions;
};

HotkeyManager::HotkeyManager(std::shared_ptr<const IParentWidgetProvider> parentWidgetProvider, std::shared_ptr<const IUiFactory> uiFactory, std::shared_ptr<ISettings> settings)
	: m_impl(std::move(parentWidgetProvider), std::move(uiFactory), std::move(settings))
{
}

HotkeyManager::~HotkeyManager() = default;

IDataItem::Ptr HotkeyManager::GetRootDataItem()
{
	return m_impl->GetRootDataItem();
}

bool HotkeyManager::HasHotkey(const QString& key) const noexcept
{
	return m_impl->HasHotkey(key);
}

QVariant HotkeyManager::GetIcon(const QString& key) const
{
	return m_impl->GetIcon(key);
}

void HotkeyManager::Add(const QString& rootKey, QWidget& widget, const QString& title)
{
	m_impl->Add(rootKey, widget, title);
}

void HotkeyManager::Add(const QString& rootKey, QMenuBar& menuBar, const QString& title)
{
	m_impl->Add(rootKey, menuBar, title);
}

void HotkeyManager::Add(const QString& rootKey, QComboBox& comboBox, const QString& title)
{
	m_impl->Add(rootKey, comboBox, title);
}

QString HotkeyManager::Set(const QString& key, const QString& shortCut)
{
	return m_impl->Set(key, shortCut);
}

void HotkeyManager::Reset(const QString& key)
{
	m_impl->Reset(key);
}

std::expected<void, QString> HotkeyManager::SetIcon(const QString& key, const QString& path)
{
	return m_impl->SetIcon(key, path);
}

void HotkeyManager::RegisterObserver(IObserver* observer)
{
	m_impl->AddObserver(observer);
	m_impl->Register(observer);
}

void HotkeyManager::UnregisterObserver(IObserver* observer)
{
	m_impl->RemoveObserver(observer);
	m_impl->Unregister(observer);
}
