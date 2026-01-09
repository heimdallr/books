#pragma once

#include <QSortFilterProxyModel>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "interface/logic/IModelProvider.h"

namespace HomeCompa::Flibrary
{

class ISourceModelObserver // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	virtual ~ISourceModelObserver()                = default;
	virtual void OnRowsRemoved(int row, int count) = 0;
};

class ScriptSortFilterModel final
	: public QSortFilterProxyModel
	, virtual ISourceModelObserver
{
	NON_COPY_MOVABLE(ScriptSortFilterModel)

public:
	explicit ScriptSortFilterModel(const std::shared_ptr<const IModelProvider>& modelProvider, QObject* parent = nullptr);
	~ScriptSortFilterModel() override;

private: // QAbstractItemModel
	QVariant headerData(const int section, const Qt::Orientation orientation, const int role) const override;
	bool setData(const QModelIndex& index, const QVariant& value, int role) override;

private: // QSortFilterProxyModel
	bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
	bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private: // ISourceModelObserver
	void OnRowsRemoved(int row, int count) override;

private:
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_source;
};

} // namespace HomeCompa::Flibrary
