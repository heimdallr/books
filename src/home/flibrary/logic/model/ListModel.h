#pragma once

#include <QAbstractItemModel>

#include "fnd/NonCopyMovable.h"

#include "data/Types.h"

namespace HomeCompa::Flibrary {

class AbstractListModel : public QAbstractItemModel
{
protected:
	explicit AbstractListModel(QObject * parent);
};

class ListModel final : public AbstractListModel
{
	NON_COPY_MOVABLE(ListModel)

public:
	explicit ListModel(const std::shared_ptr<class AbstractModelProvider> & modelProvider, QObject * parent = nullptr);
	~ListModel() override;

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
