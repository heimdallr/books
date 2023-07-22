#pragma once

#include "fnd/NonCopyMovable.h"

#include "data/DataItem.h"

#include "BaseModel.h"

namespace HomeCompa::Flibrary {

class AbstractListModel : public BaseModel
{
protected:
	explicit AbstractListModel(const std::shared_ptr<class AbstractModelProvider> & modelProvider, QObject * parent);
};

class ListModel final : public AbstractListModel
{
	NON_COPY_MOVABLE(ListModel)

public:
	explicit ListModel(const std::shared_ptr<AbstractModelProvider> & modelProvider, QObject * parent = nullptr);
	~ListModel() override;

private: // QAbstractItemModel
	QModelIndex index(int row, int column, const QModelIndex & parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex & child) const override;
	int rowCount(const QModelIndex & parent = QModelIndex()) const override;
	int columnCount(const QModelIndex & parent = QModelIndex()) const override;
};

}
