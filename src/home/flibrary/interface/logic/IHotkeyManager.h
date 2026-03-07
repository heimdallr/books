#pragma once

#include <QString>

#include "IDataItem.h"

class QAction;
class QComboBox;
class QMenuBar;

namespace HomeCompa::Flibrary
{

class IHotkeyManager // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	class IBookMenuProvider // NOLINT(cppcoreguidelines-special-member-functions)
	{
	public:
		using RequestMenuCallback = std::function<void(const QString& id, const IDataItem::Ptr& item)>;

	public:
		virtual ~IBookMenuProvider() = default;

		virtual void RequestBookMenu(RequestMenuCallback callback) = 0;
	};

public:
	virtual ~IHotkeyManager() = default;

public:
	[[nodiscard]] virtual IDataItem::Ptr GetRootDataItem()                            = 0;
	[[nodiscard]] virtual bool           HasHotkey(const QString& key) const noexcept = 0;

	virtual void Add(const QMenuBar& menuBar, const QString& title)       = 0;
	virtual void Add(QComboBox& comboBox, const QString& title)           = 0;
	virtual void Set(const QString& key, const QString& shortCut)         = 0;
	virtual bool Reset(const QString& key)                                = 0;
	virtual void SetBookMenuProvider(IBookMenuProvider* bookMenuProvider) = 0;
};

} // namespace HomeCompa::Flibrary
