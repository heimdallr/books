#pragma once

#include <QSortFilterProxyModel>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IModelProvider.h"

namespace HomeCompa::Flibrary
{

class AbstractSortFilterProxyModel : public QSortFilterProxyModel
{
protected:
	explicit AbstractSortFilterProxyModel(QObject* parent = nullptr);
};

class SortFilterProxyModel final : public AbstractSortFilterProxyModel
{
	NON_COPY_MOVABLE(SortFilterProxyModel)

public:
	explicit SortFilterProxyModel(const std::shared_ptr<const IModelProvider>& modelProvider, QObject* parent = nullptr);
	~SortFilterProxyModel() override;

private: // QAbstractItemModel
	QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
	QVariant data(const QModelIndex& index, int role) const override;
	bool setData(const QModelIndex& index, const QVariant& value, int role) override;

private: // QSortFilterProxyModel
	bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
	bool lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const override;

private:
	bool FilterAcceptsText(const QModelIndex& index) const;
	bool FilterAcceptsLanguage(const QModelIndex& index) const;
	bool FilterAcceptsRemoved(const QModelIndex& index) const;
	bool FilterAcceptsGenres(const QModelIndex& index) const;

private:
	struct Impl;
	PropagateConstPtr<Impl> m_impl;
};

} // namespace HomeCompa::Flibrary
