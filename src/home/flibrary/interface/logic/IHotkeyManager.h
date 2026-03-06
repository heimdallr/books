#pragma once
#include "IDataItem.h"

namespace HomeCompa::Flibrary
{

class IHotkeyManager // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IHotkeyManager() = default;

public:
	[[nodiscard]] virtual IDataItem::Ptr GetRootDataItem() const noexcept = 0;
};

}
