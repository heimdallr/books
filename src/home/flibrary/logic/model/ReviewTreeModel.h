#pragma once

#include "fnd/NonCopyMovable.h"

#include "interface/logic/IModelProvider.h"

#include "TreeModel.h"

namespace HomeCompa::Flibrary
{

class ReviewTreeModel final : public TreeModel
{
	NON_COPY_MOVABLE(ReviewTreeModel)

public:
	explicit ReviewTreeModel(const std::shared_ptr<IModelProvider>& modelProvider, QObject* parent = nullptr);
	~ReviewTreeModel() override;

private: // QAbstractItemModel
	QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role) const override;

private: // BaseModel
	IDataItem* GetInternalPointer(const QModelIndex& index) const override;
};

} // namespace HomeCompa::Flibrary
