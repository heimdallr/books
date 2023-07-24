#include "TreeModel.h"

#include <plog/Log.h>

#include "data/AbstractModelProvider.h"

using namespace HomeCompa::Flibrary;

AbstractTreeModel::AbstractTreeModel(const std::shared_ptr<AbstractModelProvider> & modelProvider, QObject * parent)
	: BaseModel(modelProvider, parent)
{
}

TreeModel::TreeModel(const std::shared_ptr<AbstractModelProvider> & modelProvider, QObject * parent)
	: AbstractTreeModel(modelProvider, parent)
{
	PLOGD << "TreeModel created";
}

TreeModel::~TreeModel()
{
	PLOGD << "TreeModel destroyed";
}

QModelIndex TreeModel::index(const int row, const int column, const QModelIndex & parent) const
{
	if (!hasIndex(row, column, parent))
		return {};

	const auto * parentItem = parent.isValid() ? static_cast<DataItem *>(parent.internalPointer()) : m_data.get();
	const auto * childItem = parentItem->GetChild(row);

	return childItem ? createIndex(row, column, childItem) : QModelIndex();
}

QModelIndex TreeModel::parent(const QModelIndex & index) const
{
	if (!index.isValid())
		return {};

	const auto * childItem = static_cast<DataItem *>(index.internalPointer());
	const auto * parentItem = childItem->GetParent();

	return parentItem != m_data.get() ? createIndex(static_cast<int>(parentItem->GetRow()), 0, parentItem) : QModelIndex();
}

int TreeModel::rowCount(const QModelIndex & parent) const
{
	if (parent.column() > 0)
		return 0;

	const auto * parentItem = parent.isValid() ? static_cast<DataItem *>(parent.internalPointer()) : m_data.get();
	return static_cast<int>(parentItem->GetChildCount());
}

int TreeModel::columnCount(const QModelIndex & /*parent*/) const
{
	return m_data->GetColumnCount();
}
