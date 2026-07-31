#pragma once

#include "interface/logic/IHotkeyManager.h"
#include "interface/ui/IUiFactory.h"

#include "gutil/interface/IParentWidgetProvider.h"

#include "BooksContextMenuProvider.h"

namespace HomeCompa::Flibrary
{

class HotkeyManager final : virtual public IHotkeyManager
{
	NON_COPY_MOVABLE(HotkeyManager)

public:
	HotkeyManager(std::shared_ptr<const IParentWidgetProvider> parentWidgetProvider, std::shared_ptr<const IUiFactory> uiFactory, std::shared_ptr<ISettings> settings);
	~HotkeyManager() override;

private: // IHotkeyManager
	IDataItem::Ptr GetRootDataItem() override;
	bool           HasHotkey(const QString& key) const noexcept override;
	QIcon          GetIcon(const QString& key) const override;

	void    Add(QWidget& widget, const QString& title) override;
	void    Add(QMenuBar& menuBar, const QString& title) override;
	void    Add(QComboBox& comboBox, const QString& title) override;
	QString Set(const QString& key, const QString& shortCut) override;
	void    Reset(const QString& key) override;

	std::expected<void, QString> SetIcon(const QString& key, const QString& path) override;

	void RegisterObserver(IObserver* observer) override;
	void UnregisterObserver(IObserver* observer) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
