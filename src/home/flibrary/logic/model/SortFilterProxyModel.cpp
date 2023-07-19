#include "SortFilterProxyModel.h"

#include "interface/constants/ModelRole.h"

#include "data/AbstractModelProvider.h"

using namespace HomeCompa::Flibrary;

SortFilterProxyModel::SortFilterProxyModel(const std::shared_ptr<class AbstractModelProvider> & modelProvider, QObject * parent)
	: QSortFilterProxyModel(parent)
	, m_sourceModel(modelProvider->GetSourceModel())
{
	QSortFilterProxyModel::setSourceModel(m_sourceModel.get());
	setAutoAcceptChildRows(true);
	setRecursiveFilteringEnabled(true);
}

SortFilterProxyModel::~SortFilterProxyModel() = default;

bool SortFilterProxyModel::setData(const QModelIndex & index, const QVariant & value, const int role)
{
	if (!index.isValid())
	{
		if (role == Role::Filter)
		{
			m_filter = value.toString().simplified();
			invalidateFilter();
			return true;
		}
	}

	return m_sourceModel->setData(mapToSource(index), value, role);
}

bool SortFilterProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex & sourceParent) const
{
	if (m_filter.isEmpty())
		return true;

	const auto itemIndex = m_sourceModel->index(sourceRow, 0, sourceParent);
	assert(itemIndex.isValid());
	return itemIndex.data().toString().simplified().contains(m_filter, Qt::CaseInsensitive);
}
