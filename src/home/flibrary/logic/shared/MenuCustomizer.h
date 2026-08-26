#pragma once

#include "interface/logic/IMenuCustomizer.h"
#include "interface/ui/IUiFactory.h"

#include "gutil/interface/IParentWidgetProvider.h"

#include "BooksContextMenuProvider.h"

namespace HomeCompa::Flibrary
{

class MenuCustomizer final : virtual public IMenuCustomizer
{
	NON_COPY_MOVABLE(MenuCustomizer)

public:
	MenuCustomizer(std::shared_ptr<const IParentWidgetProvider> parentWidgetProvider, std::shared_ptr<const IUiFactory> uiFactory, std::shared_ptr<ISettings> settings);
	~MenuCustomizer() override;

private: // IMenuCustomizer
	IDataItem::Ptr GetRootDataItem() const noexcept override;
	ItemAbility    GetAbilities(const QString& key) const override;
	QString        GetHotkey(const QString& key) const override;
	QVariant       GetIcon(const QString& key) const override;
	QVariant       IsHidden(const QString& key) const override;
	QVariant       AddedToToolbar(const QString& key) const override;

	void UpdateItems() override;

	void    Add(const QString& rootKey, QWidget& widget, const QString& title) override;
	void    Add(const QString& rootKey, QMenuBar& menuBar, const QString& title) override;
	void    Add(const QString& rootKey, QComboBox& comboBox, const QString& title) override;
	QString SetHotkey(const QString& key, const QString& shortCut) override;
	void    Hide(const QString& key, bool hidden) override;
	void    AddToToolbar(const QString& key, bool add) override;

	std::expected<void, QString> SetIcon(const QString& key, const QString& path) override;

	void SetToolbarController(IToolbarController* toolbarController) noexcept override;

	void RegisterObserver(IObserver* observer) override;
	void UnregisterObserver(IObserver* observer) override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
