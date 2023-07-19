#pragma once

#include <QSortFilterProxyModel>

#include "fnd/NonCopyMovable.h"

#include "data/Types.h"

namespace HomeCompa::Flibrary {

class SortFilterProxyModel final : public QSortFilterProxyModel
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
