#include "HotkeyManager.h"

#include "data/DataItem.h"

using namespace HomeCompa::Flibrary;

class HotkeyManager::Impl
{
public:
	IDataItem::Ptr GetRootDataItem() const noexcept
	{
		return m_root;
	}

private:
	IDataItem::Ptr m_root { SettingsItem::Create() };
};

HotkeyManager::HotkeyManager() = default;

HotkeyManager::~HotkeyManager() = default;

IDataItem::Ptr HotkeyManager::GetRootDataItem() const noexcept
{
	return m_impl->GetRootDataItem();
}
