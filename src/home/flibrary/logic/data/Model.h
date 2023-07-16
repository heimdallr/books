#pragma once

#include <QAbstractItemModel>

#include "fnd/NonCopyMovable.h"

#include "Types.h"

namespace HomeCompa::Flibrary {

class IModelObserver
{
public:
	virtual ~IModelObserver() = default;
};

class AbstractTreeModel : public QAbstractItemModel
{
protected:
	explicit AbstractTreeModel(QObject * parent);
};

class TreeModel : public AbstractTreeModel
{
	NON_COPY_MOVABLE(TreeModel)

public:
	explicit TreeModel(const std::shared_ptr<class AbstractModelProvider> & modelProvider, QObject * parent = nullptr);
	~TreeModel() override;

private: // QAbstractItemModel
	QModelIndex index(int row, int column, const QModelIndex & parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex & child) const override;
	int rowCount(const QModelIndex & parent = QModelIndex()) const override;
	int columnCount(const QModelIndex & parent = QModelIndex()) const override;
	QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;

private:
	DataItem::Ptr m_data;
};

}
