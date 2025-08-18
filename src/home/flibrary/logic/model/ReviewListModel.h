#pragma once

#include "fnd/NonCopyMovable.h"

#include "interface/logic/IModelProvider.h"

#include "ListModel.h"

namespace HomeCompa::Flibrary
{

class ReviewListModel : public ListModel
{
	NON_COPY_MOVABLE(ReviewListModel)

public:
	explicit ReviewListModel(const std::shared_ptr<IModelProvider>& modelProvider, QObject* parent = nullptr);
	~ReviewListModel() override;

private: // QAbstractItemModel
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

private: // BaseModel
	IDataItem* GetInternalPointer(const QModelIndex& index) const override;
};

} // namespace HomeCompa::Flibrary
