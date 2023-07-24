#pragma once

#include <QAbstractItemModel>

#include "fnd/NonCopyMovable.h"

#include "data/DataItem.h"

namespace HomeCompa::Flibrary {

class BaseModel : public QAbstractItemModel
{
	NON_COPY_MOVABLE(BaseModel)

protected:
	explicit BaseModel(const std::shared_ptr<class AbstractModelProvider> & modelProvider, QObject * parent);
	~BaseModel() override;

protected: // QAbstractItemModel
	QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
	QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;

protected:
	DataItem::Ptr m_data;

};

}
