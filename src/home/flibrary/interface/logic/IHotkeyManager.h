#pragma once
#include "IDataItem.h"

class QMenuBar;

namespace HomeCompa::Flibrary
{

class IHotkeyManager // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IHotkeyManager() = default;

public:
	[[nodiscard]] virtual IDataItem::Ptr GetRootDataItem() const noexcept = 0;

	virtual void Add(QMenuBar& menuBar, const QString& title) = 0;
};

}
