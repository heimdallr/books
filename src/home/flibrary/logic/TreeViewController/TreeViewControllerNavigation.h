#pragma once

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IDataProvider.h"
#include "interface/logic/IDatabaseController.h"
#include "interface/logic/ILogicFactory.h"
#include "interface/ui/IUiFactory.h"

#include "AbstractTreeViewController.h"

namespace HomeCompa::Flibrary
{

class TreeViewControllerNavigation final : public AbstractTreeViewController
{
	NON_COPY_MOVABLE(TreeViewControllerNavigation)

public:
	TreeViewControllerNavigation(std::shared_ptr<ISettings> settings,
	                             const std::shared_ptr<const IModelProvider>& modelProvider,
	                             const std::shared_ptr<const ILogicFactory>& logicFactory,
	                             std::shared_ptr<INavigationInfoProvider> dataProvider,
	                             std::shared_ptr<IUiFactory> uiFactory,
	                             std::shared_ptr<IDatabaseController> databaseController);
	~TreeViewControllerNavigation() override;

public:
	void RequestNavigation(bool force) const;
	void RequestBooks(bool force) const;

private: // ITreeViewController
	[[nodiscard]] std::vector<const char*> GetModeNames() const override;
	void SetCurrentId(ItemType type, QString id) override;

private: // AbstractTreeViewController
	void OnModeChanged(const QString& mode) override;
	[[nodiscard]] int GetModeIndex(const QString& mode) const override;
	[[nodiscard]] ItemType GetItemType() const noexcept override;
	[[nodiscard]] ViewMode GetViewMode() const noexcept override;
	void RequestContextMenu(const QModelIndex& index, RequestContextMenuOptions options, RequestContextMenuCallback callback) override;
	void OnContextMenuTriggered(QAbstractItemModel* model, const QModelIndex& index, const QList<QModelIndex>& indexList, IDataItem::Ptr item) override;
	CreateNewItem GetNewItemCreator() const override;
	RemoveItems GetRemoveItems() const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
