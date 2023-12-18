#pragma once

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"
#include "AbstractTreeViewController.h"

class QString;

namespace HomeCompa {
class ISettings;
}

namespace HomeCompa::Flibrary {

class TreeViewControllerBooks final
	: public AbstractTreeViewController
{
	NON_COPY_MOVABLE(TreeViewControllerBooks)

public:
	explicit TreeViewControllerBooks(std::shared_ptr<ISettings> settings
		, std::shared_ptr<DataProvider> dataProvider
		, std::shared_ptr<IModelProvider> modelProvider
		, std::shared_ptr<class ILogicFactory> logicFactory
		, std::shared_ptr<class IAnnotationController> annotationController
	);
	~TreeViewControllerBooks() override;

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
	void OnDoubleClicked(const QModelIndex & index) const override;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
