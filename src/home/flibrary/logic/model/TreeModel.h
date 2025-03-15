#pragma once

#include "fnd/NonCopyMovable.h"

#include "interface/logic/IModelProvider.h"

#include "BaseModel.h"

namespace HomeCompa::Flibrary
{

class AbstractTreeModel : public BaseModel
{
protected:
	explicit AbstractTreeModel(const std::shared_ptr<IModelProvider>& modelProvider, QObject* parent);
};

class TreeModel final : public AbstractTreeModel
{
	NON_COPY_MOVABLE(TreeModel)

public:
	explicit TreeModel(const std::shared_ptr<IModelProvider>& modelProvider, QObject* parent = nullptr);
	~TreeModel() override;

private: // QAbstractItemModel
	QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex& index) const override;
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role) const override;
};

} // namespace HomeCompa::Flibrary
