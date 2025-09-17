#pragma once

#include "fnd/NonCopyMovable.h"

#include "interface/logic/IModelProvider.h"

#include "BaseModel.h"

namespace HomeCompa::Flibrary
{

class AbstractListModel : public BaseModel
{
protected:
	explicit AbstractListModel(const std::shared_ptr<IModelProvider>& modelProvider, QObject* parent);
};

class ListModel : public AbstractListModel
{
	NON_COPY_MOVABLE(ListModel)

public:
	explicit ListModel(const std::shared_ptr<IModelProvider>& modelProvider, QObject* parent = nullptr);
	~ListModel() override;

protected: // QAbstractItemModel
	QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex& child) const override;
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role) const override;
};

} // namespace HomeCompa::Flibrary
