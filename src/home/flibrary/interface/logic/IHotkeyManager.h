#pragma once

#include <QString>

#include "IDataItem.h"

class QAction;
class QMenuBar;

namespace HomeCompa::Flibrary
{

class IHotkeyManager // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~IHotkeyManager() = default;

public:
	[[nodiscard]] virtual IDataItem::Ptr GetRootDataItem() const noexcept             = 0;
	[[nodiscard]] virtual QAction*       GetAction(const QString& key) const noexcept = 0;

	virtual void Add(QMenuBar& menuBar, const QString& title)     = 0;
	virtual void Set(const QString& key, const QString& shortCut) = 0;
	virtual bool Reset(const QString& key)                        = 0;
};

}
