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
	IDataItem::Ptr GetRootDataItem() override;
	ItemAbility    GetAbilities(const QString& key) const override;
	bool           HasHotkey(const QString& key) const override;
	QVariant       GetIcon(const QString& key) const override;

	void    Add(const QString& rootKey, QWidget& widget, const QString& title) override;
	void    Add(const QString& rootKey, QMenuBar& menuBar, const QString& title) override;
	void    Add(const QString& rootKey, QComboBox& comboBox, const QString& title) override;
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
