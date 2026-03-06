#include "HotkeyManager.h"

#include <ranges>

#include <QIdentityProxyModel>
#include <QMenuBar>

#include "data/DataItem.h"

using namespace HomeCompa::Flibrary;

namespace
{

constexpr auto ROOT = "Hotkeys";

QString GetName(const QString& parent, const QString& child)
{
	return QString("%1/%2").arg(parent, child);
}

}

class HotkeyManager::Impl
{
	struct Item
	{
		IDataItem::Ptr item;
		QAction*       action;
	};

public:
	explicit Impl(std::shared_ptr<ISettings> settings)
		: m_settings { std::move(settings) }
	{
	}

	IDataItem::Ptr GetRootDataItem() const noexcept
	{
		return m_root;
	}

	QAction* GetAction(const QString& key) const noexcept
	{
		const auto it = m_actions.find(key);
		return it != m_actions.end() ? it->second.action : nullptr;
	}

	void Add(QMenuBar& menuBar, const QString& title)
	{
		auto& menuBarItem = m_root->AppendChild(SettingsItem::Create());
		menuBarItem->SetData(menuBar.objectName(), SettingsItem::Column::Key);
		menuBarItem->SetData(title, SettingsItem::Column::Title);

		const auto removeAmp = [](QString str) {
			return str.remove('&');
		};

		const auto addChild = [&](IDataItem& parent, const QObject& obj, QString objTitle) -> IDataItem::Ptr& {
			auto child = SettingsItem::Create();
			child->SetData(GetName(parent.GetData(SettingsItem::Column::Key), obj.objectName()), SettingsItem::Column::Key);
			child->SetData(removeAmp(std::move(objTitle)), SettingsItem::Column::Title);
			return parent.AppendChild(std::move(child));
		};

		const auto enumerate = [&](const QList<QMenu*>& menuList, IDataItem& parent, std::unordered_set<const QAction*>& menuActions, const auto& r) -> void {
			for (const auto* menu : menuList)
			{
				menuActions.emplace(menu->menuAction());
				auto& child = addChild(parent, *menu, menu->title());

				std::unordered_set<const QAction*> actions;
				r(menu->findChildren<QMenu*>(Qt::FindDirectChildrenOnly), *child, actions, r);

				for (auto* action : menu->actions() | std::views::filter([&](const QAction* item) {
										return !(item->isSeparator() || actions.contains(item));
									}))
				{
					auto& actionItem = addChild(*child, *action, action->text());
					m_actions.try_emplace(actionItem->GetData(SettingsItem::Column::Key), Item { actionItem, action });
					if (const auto shortCut = m_settings->Get(GetName(ROOT, actionItem->GetData(SettingsItem::Column::Key))); shortCut.isValid())
						action->setShortcut(QKeySequence(shortCut.toString(), QKeySequence::PortableText));
					actionItem->SetData(action->shortcut().toString(), SettingsItem::Column::Value);
					assert(!actionItem->GetData(SettingsItem::Column::Key).isEmpty());
				}
			}
		};

		std::unordered_set<const QAction*> actions;
		enumerate(menuBar.findChildren<QMenu*>(Qt::FindDirectChildrenOnly), *menuBarItem, actions, enumerate);
	}

	void Set(const QString& key, const QString& shortCut)
	{
		const auto it = m_actions.find(key);
		assert(it != m_actions.end());
		auto& [item, action] = it->second;

		item->SetData(shortCut, SettingsItem::Column::Value);
		action->setShortcut(QKeySequence(shortCut, QKeySequence::PortableText));
		m_settings->Set(GetName(ROOT, item->GetData(SettingsItem::Column::Key)), action->shortcut().toString());
	}

	bool Reset(const QString& key)
	{
		const auto it = m_actions.find(key);
		if (it == m_actions.end())
			return false;

		auto& [item, action] = it->second;

		item->SetData({}, SettingsItem::Column::Value);
		action->setShortcut(QKeySequence {});
		m_settings->Remove(GetName(ROOT, item->GetData(SettingsItem::Column::Key)));

		return true;
	}

private:
	PropagateConstPtr<ISettings, std::shared_ptr> m_settings;

	IDataItem::Ptr m_root { SettingsItem::Create() };

	std::unordered_map<QString, Item> m_actions;
};

HotkeyManager::HotkeyManager(std::shared_ptr<ISettings> settings)
	: m_impl(std::move(settings))
{
}

HotkeyManager::~HotkeyManager() = default;

IDataItem::Ptr HotkeyManager::GetRootDataItem() const noexcept
{
	return m_impl->GetRootDataItem();
}

QAction* HotkeyManager::GetAction(const QString& key) const noexcept
{
	return m_impl->GetAction(key);
}

void HotkeyManager::Add(QMenuBar& menuBar, const QString& title)
{
	m_impl->Add(menuBar, title);
}

void HotkeyManager::Set(const QString& key, const QString& shortCut)
{
	m_impl->Set(key, shortCut);
}

bool HotkeyManager::Reset(const QString& key)
{
	return m_impl->Reset(key);
}
