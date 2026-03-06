#pragma once

#include "interface/logic/IHotkeyManager.h"

#include "BooksContextMenuProvider.h"

namespace HomeCompa::Flibrary
{

class HotkeyManager final : virtual public IHotkeyManager
{
	NON_COPY_MOVABLE(HotkeyManager)

public:
	HotkeyManager();
	~HotkeyManager() override;

private: // IHotkeyManager
	IDataItem::Ptr GetRootDataItem() const noexcept override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
