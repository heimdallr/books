#include "HotkeyManager.h"

#include <QAction>
#include <QEvent>
#include <QEventLoop>
#include <QShortcut>

#include "fnd/observable.h"

#include "interface/constants/SettingsConstant.h"
#include "interface/localization.h"

#include "data/DataItem.h"

#include "BookInteractor.h"
#include "log.h"

using namespace HomeCompa::Flibrary;
using namespace HomeCompa;

namespace
{

constexpr auto CONTEXT = "HotkeyManager";

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
QString SetShortCutImpl(T* obj, const QKeySequence& value) = delete;

template <>
QString SetShortCutImpl<QAction>(QAction* obj, const QKeySequence& value)
{
	obj->setShortcut(value);
	return obj->shortcut().toString(QKeySequence::PortableText);
}

template <>
QString SetShortCutImpl<QShortcut>(QShortcut* obj, const QKeySequence& value)
{
	obj->setKey(value);
	return obj->key().toString(QKeySequence::PortableText);
}

} // namespace

class HotkeyManager::Impl
	: public QObject
	, public Observable<IObserver>
{
	struct Item
	{
		IDataItem::Ptr item;
		QAction*       action { nullptr };
		QShortcut*     shortcut { nullptr };
		IObserver*     observer { nullptr };

		QString SetShortCut(const QString& value = {})
		{
			item->SetData(value, SettingsItem::Column::Value);

			const auto keySequence = value.isEmpty() ? QKeySequence {} : QKeySequence(value, QKeySequence::PortableText);

			if (action)
				return SetShortCutImpl(action, keySequence);

			if (!shortcut)
			{
				if (value.isEmpty())
					return value;

				assert(observer);
				shortcut = new QShortcut(keySequence, observer->GetParentObject());
				QObject::connect(shortcut, &QShortcut::activated, [o = observer, key = item->GetData(SettingsItem::Column::Key)] {
					o->OnHotkeyActivated(key);
				});
			}

			return SetShortCutImpl(shortcut, keySequence);
		}
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
		return it != m_actions.end();
	}

	void Add(QWidget& widget, const QString& title)
	{
		QString* itemKey = nullptr;
		auto     item    = m_uiFactory->AddWidgetToHotkeys(widget, title, [&](const IDataItem::Ptr& actionItem, QAction* action, QObject* parent) {
			itemKey = &Add(actionItem, Item { .item = actionItem, .action = action }, parent);
		});
		assert(itemKey);
		*itemKey = item->GetData(SettingsItem::Column::Key);
		m_root->AppendChild(std::move(item));
	}

	void Add(QMenuBar& menuBar, const QString& title)
	{
		QString* itemKey = nullptr;
		auto     item    = m_uiFactory->AddMenuBarToHotkeys(menuBar, title, [&](const IDataItem::Ptr& actionItem, QAction* action, QObject* parent) {
			itemKey = &Add(actionItem, Item { .item = actionItem, .action = action }, parent);
		});
		assert(itemKey);
		*itemKey = item->GetData(SettingsItem::Column::Key);
		m_root->AppendChild(std::move(item));
	}

	void Add(QComboBox& comboBox, const QString& title)
	{
		QString* itemKey = nullptr;
		auto     item    = m_uiFactory->AddComboBoxToHotkeys(comboBox, title, [&](const IDataItem::Ptr& actionItem, QShortcut* shortcut, QObject* parent) {
			itemKey = &Add(actionItem, Item { .item = actionItem, .shortcut = shortcut }, parent);
		});
		assert(itemKey);
		*itemKey = item->GetData(SettingsItem::Column::Key);
		m_root->AppendChild(std::move(item));
	}

	void Set(const QString& key, const QString& shortCut)
	{
		const auto it = m_actions.find(key);
		assert(it != m_actions.end());

		auto value = it->second.SetShortCut(shortCut);
		m_settings->Set(GetName(Constant::Settings::HOTKEYS_ROOT, it->second.item->GetData(SettingsItem::Column::Key)), value);
	}

	bool Reset(const QString& key)
	{
		const auto it = m_actions.find(key);
		if (it == m_actions.end())
			return false;

		it->second.SetShortCut();
		m_settings->Remove(GetName(Constant::Settings::HOTKEYS_ROOT, it->second.item->GetData(SettingsItem::Column::Key)));

		return true;
	}

	void AddObserver(IObserver* observer)
	{
		auto& actionsKeys = Add(observer->GetParentObject());
		actionsKeys.first = observer->GetKey();

		const auto enumerate = [&](const QString& key, const auto& r) -> void {
			for (const auto& k : m_settings->GetKeys())
				if (const auto shortCut = m_settings->Get(k); shortCut.isValid())
				{
					auto child = SettingsItem::Create();
					child->SetData(GetName(key, k), SettingsItem::Column::Key);
					auto&& [actionKey, shortCutItem] = *m_actions
					                                        .try_emplace(
																child->GetData(SettingsItem::Column::Key),
																Item {
																	.item     = child,
																	.observer = observer,
																}
															)
					                                        .first;
					actionsKeys.second.emplace_back(actionKey);
					shortCutItem.SetShortCut(shortCut.toString());
				}

			for (const auto& group : m_settings->GetGroups())
			{
				const SettingsGroup settingsSubGroup(*m_settings, group);
				r(GetName(key, group), r);
			}
		};

		const SettingsGroup settingsGroup(*m_settings, GetName(Constant::Settings::HOTKEYS_ROOT, observer->GetKey()));
		enumerate(observer->GetKey(), enumerate);

		Register(observer);
	}

	void RemoveObserver(IObserver* observer)
	{
		Remove(observer->GetParentObject());
		Unregister(observer);
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

		for (const auto& key : it->second.second)
			if (const auto itAction = m_actions.find(key); itAction != m_actions.end())
				if (auto* shortcut = itAction->second.shortcut)
					shortcut->setEnabled(enabled);
	}

	QString& Add(const IDataItem::Ptr& actionItem, Item item, QObject* parent)
	{
		auto  key              = actionItem->GetData(SettingsItem::Column::Key);
		auto& actionsKeys      = Add(parent);
		const auto [it, added] = m_actions.try_emplace(std::move(key), std::move(item));
		assert(added);
		actionsKeys.second.emplace_back(it->first);
		return actionsKeys.first;
	}

	std::pair<QString, std::vector<QString>>& Add(QObject* parent)
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

		if (const auto menu = m_root->FindChild([&key = it->second.first](const auto& item) {
				return item.GetData(SettingsItem::Column::Key) == key;
			}))
			m_root->RemoveChild(menu->GetRow());

		for (const auto& key : it->second.second)
			m_actions.erase(key);
		m_objToActions.erase(it);
	}

	void UpdateObserverMenu()
	{
		Perform([this](const IObserver* observer) {
			if (const auto bookMenu = m_root->FindChild([key = observer->GetKey()](const auto& item) {
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
					Add(dstChild, Item { .item = dstChild, .observer = observer }, observer->GetParentObject());

				r(*srcChild, *dstChild, r);
			}
		};

		const auto& bookItem = m_root->AppendChild(SettingsItem::Create());
		bookItem->SetData(observer->GetKey(), SettingsItem::Column::Key);
		bookItem->SetData(Tr(observer->GetKey()), SettingsItem::Column::Title);
		enumerate(*item, *bookItem, enumerate);
	}

private:
	std::shared_ptr<const IParentWidgetProvider>  m_parentWidgetProvider;
	std::shared_ptr<const IUiFactory>             m_uiFactory;
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;

	IDataItem::Ptr m_root { SettingsItem::Create() };

	std::map<QString, Item> m_actions;

	std::unordered_map<const QObject*, std::pair<QString, std::vector<QString>>> m_objToActions;
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

void HotkeyManager::Add(QWidget& widget, const QString& title)
{
	m_impl->Add(widget, title);
}

void HotkeyManager::Add(QMenuBar& menuBar, const QString& title)
{
	m_impl->Add(menuBar, title);
}

void HotkeyManager::Add(QComboBox& comboBox, const QString& title)
{
	m_impl->Add(comboBox, title);
}

void HotkeyManager::Set(const QString& key, const QString& shortCut)
{
	m_impl->Set(key, shortCut);
}

bool HotkeyManager::Reset(const QString& key)
{
	return m_impl->Reset(key);
}

void HotkeyManager::RegisterObserver(IObserver* observer)
{
	m_impl->AddObserver(observer);
}

void HotkeyManager::UnregisterObserver(IObserver* observer)
{
	m_impl->RemoveObserver(observer);
}
