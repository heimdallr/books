#pragma once

#include <QSortFilterProxyModel>

#include "fnd/memory.h"
#include "fnd/NonCopyMovable.h"

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
	explicit SortFilterProxyModel(const std::shared_ptr<class AbstractModelProvider> & modelProvider
		, QObject * parent = nullptr);
	~SortFilterProxyModel() override;

private: // QAbstractItemModel
	QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
	QVariant data(const QModelIndex& index, int role) const override;
	bool setData(const QModelIndex& index, const QVariant& value, int role) override;

private: // QSortFilterProxyModel
	bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
	bool filterAcceptsText(const QModelIndex & index) const;
	bool filterAcceptsLanguage(const QModelIndex & index) const;
	bool filterAcceptsRemoved(const QModelIndex & index) const;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
