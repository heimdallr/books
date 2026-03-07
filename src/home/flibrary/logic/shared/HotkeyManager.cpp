#include "HotkeyManager.h"

#include <ranges>

#include <QComboBox>
#include <QMenuBar>
#include <QShortcut>

#include "data/DataItem.h"

#include "BookInteractor.h"
#include "log.h"

using namespace HomeCompa::Flibrary;

namespace
{

constexpr auto ROOT = "ui/Hotkeys";

QString GetName(const QString& parent, const QString& child)
{
	return QString("%1/%2").arg(parent, child);
}

template <typename T>
QString SetShortCutImpl(T* obj, const QKeySequence& value) = delete;

template <>
QString SetShortCutImpl<QAction>(QAction* obj, const QKeySequence& value)
{
	obj->setShortcut(value);
	return obj->shortcut().toString(QKeySequence::PortableText);
};
template <>
QString SetShortCutImpl<QShortcut>(QShortcut* obj, const QKeySequence& value)
{
	obj->setKey(value);
	return obj->key().toString(QKeySequence::PortableText);
}

}

class HotkeyManager::Impl
{
	struct Item
	{
		IDataItem::Ptr item;
		QAction*       action { nullptr };
		QShortcut*     shortcut { nullptr };

		QString SetShortCut(const QString& value = {}) const
		{
			item->SetData(value, SettingsItem::Column::Value);

			const auto keySequence = value.isEmpty() ? QKeySequence {} : QKeySequence(value, QKeySequence::PortableText);

			if (action)
				return SetShortCutImpl(action, keySequence);

			if (shortcut)
				return SetShortCutImpl(shortcut, keySequence);

			assert(false && "bad logic");
			return value;
		}
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

	bool HasHotkey(const QString& key) const noexcept
	{
		const auto it = m_actions.find(key);
		return it != m_actions.end();
	}

	void Add(const QMenuBar& menuBar, const QString& title)
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
				if (menu->title().isEmpty())
					continue;

				menuActions.emplace(menu->menuAction());
				auto& child = addChild(parent, *menu, menu->title());

				std::unordered_set<const QAction*> actions;
				r(menu->findChildren<QMenu*>(Qt::FindDirectChildrenOnly), *child, actions, r);

				for (auto* action : menu->actions() | std::views::filter([&](const QAction* item) {
										return !(item->isSeparator() || actions.contains(item));
									}))
				{
					if (action->objectName().isEmpty())
					{
						PLOGW << action->text() << ": objectName is empty";
						continue;
					}

					auto& actionItem = addChild(*child, *action, action->text());
					m_actions.try_emplace(actionItem->GetData(SettingsItem::Column::Key), Item { actionItem, action });
					if (const auto shortCut = m_settings->Get(GetName(ROOT, actionItem->GetData(SettingsItem::Column::Key))); shortCut.isValid())
						action->setShortcut(QKeySequence(shortCut.toString(), QKeySequence::PortableText));
					actionItem->SetData(action->shortcut().toString(), SettingsItem::Column::Value);
				}
			}
		};

		std::unordered_set<const QAction*> actions;
		enumerate(menuBar.findChildren<QMenu*>(Qt::FindDirectChildrenOnly), *menuBarItem, actions, enumerate);
	}

	void Add(QComboBox& comboBox, const QString& title)
	{
		auto& comboBoxItem = m_root->AppendChild(SettingsItem::Create());
		comboBoxItem->SetData(comboBox.objectName(), SettingsItem::Column::Key);
		comboBoxItem->SetData(title, SettingsItem::Column::Title);

		for (int i = 0, sz = comboBox.count(); i < sz; ++i)
		{
			auto& child = comboBoxItem->AppendChild(SettingsItem::Create());
			child->SetData(GetName(comboBoxItem->GetData(SettingsItem::Column::Key), comboBox.itemData(i).toString()), SettingsItem::Column::Key);
			child->SetData(comboBox.itemText(i), SettingsItem::Column::Title);

			auto* shortcut = new QShortcut(&comboBox);
			shortcut->setObjectName(comboBox.itemData(i).toString());
			QObject::connect(shortcut, &QShortcut::activated, [comboBox = &comboBox, i] {
				comboBox->setCurrentIndex(i);
			});
			m_actions.try_emplace(child->GetData(SettingsItem::Column::Key), Item { .item = child, .shortcut = shortcut });
			if (const auto shortCut = m_settings->Get(GetName(ROOT, child->GetData(SettingsItem::Column::Key))); shortCut.isValid())
				shortcut->setKey(QKeySequence(shortCut.toString(), QKeySequence::PortableText));
			child->SetData(shortcut->key().toString(), SettingsItem::Column::Value);
		}
	}

	void Set(const QString& key, const QString& shortCut)
	{
		const auto it = m_actions.find(key);
		assert(it != m_actions.end());

		auto value = it->second.SetShortCut(shortCut);
		m_settings->Set(GetName(ROOT, it->second.item->GetData(SettingsItem::Column::Key)), value);
	}

	bool Reset(const QString& key)
	{
		const auto it = m_actions.find(key);
		if (it == m_actions.end())
			return false;

		it->second.SetShortCut();
		m_settings->Remove(GetName(ROOT, it->second.item->GetData(SettingsItem::Column::Key)));

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

bool HotkeyManager::HasHotkey(const QString& key) const noexcept
{
	return m_impl->HasHotkey(key);
}

void HotkeyManager::Add(const QMenuBar& menuBar, const QString& title)
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
