#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "AbstractTreeViewController.h"

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Flibrary {

class TreeViewControllerNavigation final
	: public AbstractTreeViewController
{
	NON_COPY_MOVABLE(TreeViewControllerNavigation)

public:
	TreeViewControllerNavigation(std::shared_ptr<ISettings> settings
		, std::shared_ptr<DataProvider> dataProvider
		, std::shared_ptr<IModelProvider> modelProvider
		, std::shared_ptr<class ILogicFactory> logicFactory
		, std::shared_ptr<class IUiFactory> uiFactory
		, std::shared_ptr<class DatabaseController> databaseController
	);
	~TreeViewControllerNavigation() override;

public:
	void RequestNavigation() const;
	void RequestBooks(bool force) const;

private: // ITreeViewController
	[[nodiscard]] std::vector<const char *> GetModeNames() const override;
	void SetCurrentId(QString id) override;

private: // AbstractTreeViewController
	void OnModeChanged(const QString & mode) override;
	[[nodiscard]] int GetModeIndex(const QString & mode) const override;
	[[nodiscard]] ItemType GetItemType() const noexcept override;
	[[nodiscard]] ViewMode GetViewMode() const noexcept override;
	void RequestContextMenu(const QModelIndex & index, RequestContextMenuCallback callback) override;
	void OnContextMenuTriggered(QAbstractItemModel * model, const QModelIndex & index, const QList<QModelIndex> & indexList, IDataItem::Ptr item) override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
