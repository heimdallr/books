#pragma once

#include <QSortFilterProxyModel>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

#include "data/DataItem.h"

namespace HomeCompa::Flibrary {

class AbstractSortFilterProxyModel : public QSortFilterProxyModel
{
protected:
	explicit AbstractSortFilterProxyModel(QObject * parent = nullptr);
};

class SortFilterProxyModel final : public AbstractSortFilterProxyModel
{
	NON_COPY_MOVABLE(SortFilterProxyModel)

public:
	explicit SortFilterProxyModel(const std::shared_ptr<class AbstractModelProvider> & modelProvider, QObject * parent = nullptr);
	~SortFilterProxyModel() override;

private: // QAbstractItemModel
	bool setData(const QModelIndex& index, const QVariant& value, int role) override;

private: // QSortFilterProxyModel
	bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_sourceModel;
	QString m_filter;
};

}
